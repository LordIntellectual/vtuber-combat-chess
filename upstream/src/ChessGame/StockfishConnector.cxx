#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <iostream>
#include <algorithm>
#include <vector>
#include <string.h>
#include <errno.h>
#include <poll.h>

#include "../utils/utils.hxx"

#include "ConnectionException.hxx"

#include "StockfishConnector.hxx"

/* Read a complete line from a pipe and return it as a string. */
std::string readLine(FILE* readPipe, bool print){
  std::string line = "";
  char buffer[256] = "";

  do{
    memset(buffer, 0, sizeof buffer);

    if(fgets(buffer, sizeof buffer, readPipe) == NULL){
      if(print) std::cerr << "[Stockfish] readLine: EOF/error on engine pipe\n";
      throw ConnectionException(
        "Stockfish pipe closed while reading (engine crashed or hung)");
    }

    line.append(buffer);
  }while(line.empty() || line.back() != '\n');

  if(print) std::cout << line << std::flush;

  return line;
};

void writeLine(FILE* writePipe, std::string line, bool print){
  if(!writePipe){
    throw ConnectionException("Stockfish write pipe is null");
  }
  if(fwrite(line.c_str(), 1, line.size(), writePipe) != line.size()){
    throw ConnectionException("Failed to write to Stockfish pipe");
  }
  if(fflush(writePipe) != 0){
    throw ConnectionException("Failed to flush Stockfish pipe");
  }

  if(print) std::cout << line << std::flush;
};

static int movetimeMsForSkill(int skill){
  if(skill <= 0) return 200;
  if(skill <= 5) return 400;
  if(skill <= 10) return 800;
  if(skill <= 15) return 1200;
  return 2000;
}

StockfishConnector::StockfishConnector()
  : parentWritePipeF(NULL), parentReadPipeF(NULL),
    childPid(-1), moves{""}, engineAlive(false) {}

void StockfishConnector::closePipes(){
  if(parentReadPipeF){
    int fd = fileno(parentReadPipeF);
    fclose(parentReadPipeF);
    parentReadPipeF = NULL;
    if(fd >= 0) close(fd);
  }
  if(parentWritePipeF){
    int fd = fileno(parentWritePipeF);
    fclose(parentWritePipeF);
    parentWritePipeF = NULL;
    if(fd >= 0) close(fd);
  }
}

void StockfishConnector::killChild(){
  if(childPid > 0){
    kill(childPid, SIGTERM);
    int status = 0;
    // Reap without hanging forever
    for(int i = 0; i < 20; i++){
      pid_t r = waitpid(childPid, &status, WNOHANG);
      if(r == childPid || r < 0) break;
      usleep(10000);
    }
    if(waitpid(childPid, &status, WNOHANG) == 0){
      kill(childPid, SIGKILL);
      waitpid(childPid, &status, 0);
    }
    childPid = -1;
  } else {
    // Reap any zombies
    int status = 0;
    while(waitpid(-1, &status, WNOHANG) > 0) {}
  }
  engineAlive = false;
}

void StockfishConnector::restartCommunication(){
  std::cerr << "[Stockfish] Restarting engine…\n";
  // Try graceful quit if pipes still open
  if(parentWritePipeF){
    try{
      writeLine(parentWritePipeF, "quit\n", false);
    }catch(...){}
  }
  closePipes();
  killChild();
  startCommunication();
}

void StockfishConnector::startCommunication(){
  // Clean any previous session
  closePipes();
  if(childPid > 0) killChild();
  engineAlive = false;

  const char* readMode = "r";
  const char* writeMode = "w";

  int fd[2];
  if(pipe(fd) == -1){
    throw ConnectionException("Failed to create pipes");
  }

  int childReadPipe   = fd[0];
  int parentWritePipe = fd[1];

  if(pipe(fd) == -1){
    throw ConnectionException("Failed to create pipes");
  }

  int parentReadPipe = fd[0];
  int childWritePipe = fd[1];

  pid_t pid = fork();

  if(pid < 0){
    throw ConnectionException("Failed to fork process");
  }

  if(pid == 0){
    dup2(childReadPipe, fileno(stdin));
    dup2(childWritePipe, fileno(stdout));

    close(parentReadPipe);
    close(parentWritePipe);
    close(childReadPipe);

    execlp("stockfish", "stockfish", (char *)NULL);

    writeLine(fdopen(childWritePipe, writeMode), "stop\n", true);
    close(childWritePipe);
    _exit(127);
  }

  childPid = pid;
  close(childReadPipe);
  close(childWritePipe);

  parentReadPipeF = fdopen(parentReadPipe, readMode);
  parentWritePipeF = fdopen(parentWritePipe, writeMode);
  setvbuf(parentWritePipeF, NULL, _IOLBF, 0);

  std::string line;
  std::vector<std::string> splittedLine;

  line = readLine(parentReadPipeF, true);
  splittedLine = split(line, ' ');
  if(splittedLine.empty() || splittedLine.at(0).compare("Stockfish") != 0)
    throw ConnectionException(
    "Communication with stockfish did'nt start properly, closing");

  writeLine(parentWritePipeF, "uci\n", true);
  while(true){
    line = readLine(parentReadPipeF, false);
    if(line.compare(0, 5, "uciok") == 0) break;
  }

  std::string difficultyOption = "setoption name Skill Level value ";
  difficultyOption.append(std::to_string(difficultyLevel));
  difficultyOption.append("\n");
  writeLine(parentWritePipeF, difficultyOption, true);

  // Cap threads/hash — keeps engine lighter under heavy 3D load
  writeLine(parentWritePipeF, "setoption name Threads value 1\n", false);
  writeLine(parentWritePipeF, "setoption name Hash value 64\n", false);

  writeLine(parentWritePipeF, "ucinewgame\n", true);
  writeLine(parentWritePipeF, "isready\n", true);

  while(true){
    line = readLine(parentReadPipeF, true);
    if(line.compare(0, 7, "readyok") == 0) break;
  }

  engineAlive = true;
  moves = "";
  std::cout << "[Stockfish] Engine ready (pid=" << childPid << ")\n";
}

std::string StockfishConnector::getNextAIMoveFromFen(const std::string& fen){
  // Auto-recover if a previous crash left us without a live engine
  if(!isAlive()){
    restartCommunication();
  }

  auto askOnce = [&]() -> std::string {
    std::string line;
    std::vector<std::string> splittedLine;

    std::cout << std::endl << "[Stockfish] position fen " << fen << std::endl;

    // Stop any leftover search, then set position and go
    writeLine(parentWritePipeF, "stop\n", false);

    const int ms = movetimeMsForSkill(difficultyLevel);
    line = "position fen ";
    line.append(fen);
    line.append("\nisready\n");
    writeLine(parentWritePipeF, line, true);

    // Drain until readyok (also discards residual info lines)
    while(true){
      line = readLine(parentReadPipeF, false);
      if(line.compare(0, 7, "readyok") == 0) break;
    }

    line = "go movetime ";
    line.append(std::to_string(ms));
    line.append("\n");
    writeLine(parentWritePipeF, line, true);

    while(true){
      line = readLine(parentReadPipeF, false);
      splittedLine = split(line, ' ');
      if(!splittedLine.empty() && splittedLine.at(0).compare("bestmove") == 0) break;
    }

    if(splittedLine.size() < 2){
      throw ConnectionException("Stockfish bestmove line had no move");
    }
    std::string aiMove = splittedLine.at(1);
    aiMove.erase(std::remove(aiMove.begin(), aiMove.end(), '\n'), aiMove.end());
    if(aiMove.empty() || aiMove == "(none)"){
      throw ConnectionException("Stockfish returned no legal move (checkmate/stalemate?)");
    }

    std::cout << "AI move: " << aiMove << std::endl;

    if(splittedLine.size() >= 4 && splittedLine.at(2) == "ponder"){
      suggestedUserMove = splittedLine.at(3);
      std::cout << "Suggested user move: " << suggestedUserMove << std::endl;
    }else{
      suggestedUserMove = "(none)";
    }

    moves = fen + " -> " + aiMove;
    return aiMove;
  };

  try {
    return askOnce();
  } catch (const ConnectionException& e) {
    std::cerr << "[Stockfish] " << e.what() << " — attempting restart\n";
    engineAlive = false;
    try {
      restartCommunication();
      return askOnce();
    } catch (const std::exception& e2) {
      engineAlive = false;
      throw ConnectionException(
        std::string("Stockfish failed after restart: ") + e2.what());
    }
  }
}

std::string StockfishConnector::getNextAIMove(std::string userMove){
  if(!isAlive()) restartCommunication();

  std::string line;
  std::vector<std::string> splittedLine;

  std::cout << std::endl << "User move: " << userMove << std::endl;

  moves.append(userMove);
  moves.append(" ");

  const int ms = movetimeMsForSkill(difficultyLevel);
  line = "position startpos moves ";
  line.append(moves);
  line.append("\ngo movetime ");
  line.append(std::to_string(ms));
  line.append("\n");
  writeLine(parentWritePipeF, line, true);

  while(true){
    line = readLine(parentReadPipeF, false);
    splittedLine = split(line, ' ');
    if(!splittedLine.empty() && splittedLine.at(0).compare("bestmove") == 0) break;
  }

  if(splittedLine.size() < 2){
    throw ConnectionException("Stockfish bestmove line had no move");
  }
  std::string aiMove = splittedLine.at(1);
  aiMove.erase(std::remove(aiMove.begin(), aiMove.end(), '\n'), aiMove.end());
  if(aiMove.empty() || aiMove == "(none)"){
    if(moves.size() >= userMove.size() + 1){
      moves.resize(moves.size() - (userMove.size() + 1));
    }
    throw ConnectionException("Stockfish returned no legal move");
  }
  moves.append(aiMove);
  moves.append(" ");

  std::cout << "AI move: " << aiMove << std::endl;

  if(splittedLine.size() >= 4 && splittedLine.at(2) == "ponder"){
    suggestedUserMove = splittedLine.at(3);
    std::cout << "Suggested user move: " << suggestedUserMove << std::endl;
  }else{
    suggestedUserMove = "(none)";
  }

  return aiMove;
}

StockfishConnector::~StockfishConnector(){
  if(parentWritePipeF){
    try{
      writeLine(parentWritePipeF, "quit\n", true);
    }catch(...){}
  }
  closePipes();
  killChild();
}
