#include <iostream>
#include <algorithm>
#include <vector>
#include <string.h>
#include <errno.h>
#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <poll.h>
#endif

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
  }while(line.empty() || (line.back() != '\n' && line.back() != '\r'));

  // Normalize CRLF from Windows engines
  while(!line.empty() && (line.back() == '\n' || line.back() == '\r'))
    line.pop_back();
  line.push_back('\n');

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

/** Resolve stockfish executable name/path for CreateProcess / execlp. */
static std::string stockfishCommand(){
  // Optional override: full path to the engine binary
  const char* env = std::getenv("VCC_STOCKFISH");
  if(env && env[0]) return std::string(env);
  env = std::getenv("STOCKFISH");
  if(env && env[0]) return std::string(env);
#ifdef _WIN32
  return "stockfish.exe";
#else
  return "stockfish";
#endif
}

StockfishConnector::StockfishConnector()
  : parentWritePipeF(NULL), parentReadPipeF(NULL),
#ifdef _WIN32
    childProcess(NULL), childThread(NULL),
#else
    childPid(-1),
#endif
    moves{""}, engineAlive(false) {}

void StockfishConnector::closePipes(){
  if(parentReadPipeF){
    fclose(parentReadPipeF);
    parentReadPipeF = NULL;
  }
  if(parentWritePipeF){
    fclose(parentWritePipeF);
    parentWritePipeF = NULL;
  }
}

void StockfishConnector::killChild(){
#ifdef _WIN32
  if(childProcess){
    // Soft stop already attempted via "quit"; force if still running
    if(WaitForSingleObject(childProcess, 200) == WAIT_TIMEOUT){
      TerminateProcess(childProcess, 1);
      WaitForSingleObject(childProcess, 2000);
    }
    CloseHandle(childProcess);
    childProcess = NULL;
  }
  if(childThread){
    CloseHandle(childThread);
    childThread = NULL;
  }
#else
  if(childPid > 0){
    kill(childPid, SIGTERM);
    int status = 0;
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
    int status = 0;
    while(waitpid(-1, &status, WNOHANG) > 0) {}
  }
#endif
  engineAlive = false;
}

void StockfishConnector::restartCommunication(){
  std::cerr << "[Stockfish] Restarting engine…\n";
  if(parentWritePipeF){
    try{
      writeLine(parentWritePipeF, "quit\n", false);
    }catch(...){}
  }
  closePipes();
  killChild();
  startCommunication();
}

#ifdef _WIN32
static void startCommunicationWindows(StockfishConnector* self,
                                      FILE*& parentReadPipeF,
                                      FILE*& parentWritePipeF,
                                      HANDLE& childProcess,
                                      HANDLE& childThread){
  SECURITY_ATTRIBUTES sa;
  ZeroMemory(&sa, sizeof(sa));
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;
  sa.lpSecurityDescriptor = NULL;

  HANDLE childStdInRd = NULL, childStdInWr = NULL;
  HANDLE childStdOutRd = NULL, childStdOutWr = NULL;

  if(!CreatePipe(&childStdInRd, &childStdInWr, &sa, 0))
    throw ConnectionException("Failed to create Stockfish stdin pipe");
  if(!SetHandleInformation(childStdInWr, HANDLE_FLAG_INHERIT, 0))
    throw ConnectionException("Failed to configure Stockfish stdin pipe");

  if(!CreatePipe(&childStdOutRd, &childStdOutWr, &sa, 0))
    throw ConnectionException("Failed to create Stockfish stdout pipe");
  if(!SetHandleInformation(childStdOutRd, HANDLE_FLAG_INHERIT, 0))
    throw ConnectionException("Failed to configure Stockfish stdout pipe");

  STARTUPINFOA si;
  PROCESS_INFORMATION pi;
  ZeroMemory(&si, sizeof(si));
  ZeroMemory(&pi, sizeof(pi));
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdInput = childStdInRd;
  si.hStdOutput = childStdOutWr;
  si.hStdError = childStdOutWr; // merge stderr so banner is not lost

  std::string cmd = stockfishCommand();
  // CreateProcess may modify the command line buffer
  std::vector<char> cmdBuf(cmd.begin(), cmd.end());
  cmdBuf.push_back('\0');

  BOOL ok = CreateProcessA(
    NULL,
    cmdBuf.data(),
    NULL,
    NULL,
    TRUE, // inherit handles
    CREATE_NO_WINDOW,
    NULL,
    NULL,
    &si,
    &pi);

  // Parent no longer needs the child-end handles
  CloseHandle(childStdInRd);
  CloseHandle(childStdOutWr);

  if(!ok){
    CloseHandle(childStdInWr);
    CloseHandle(childStdOutRd);
    DWORD err = GetLastError();
    throw ConnectionException(
      std::string("Failed to start Stockfish (") + cmd +
      "). Place stockfish.exe next to the game or on PATH, or set VCC_STOCKFISH. "
      "Win32 error " + std::to_string(err));
  }

  childProcess = pi.hProcess;
  childThread = pi.hThread;

  int rfd = _open_osfhandle(reinterpret_cast<intptr_t>(childStdOutRd), _O_RDONLY | _O_BINARY);
  int wfd = _open_osfhandle(reinterpret_cast<intptr_t>(childStdInWr), _O_WRONLY | _O_BINARY);
  if(rfd < 0 || wfd < 0){
    throw ConnectionException("Failed to map Stockfish pipes to FILE*");
  }
  parentReadPipeF = _fdopen(rfd, "rb");
  parentWritePipeF = _fdopen(wfd, "wb");
  if(!parentReadPipeF || !parentWritePipeF){
    throw ConnectionException("Failed to fdopen Stockfish pipes");
  }
  setvbuf(parentWritePipeF, NULL, _IONBF, 0);

  (void)self;
}
#endif

void StockfishConnector::startCommunication(){
  closePipes();
#ifdef _WIN32
  if(childProcess) killChild();
#else
  if(childPid > 0) killChild();
#endif
  engineAlive = false;

#ifdef _WIN32
  startCommunicationWindows(this, parentReadPipeF, parentWritePipeF,
                            childProcess, childThread);
#else
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

    std::string cmd = stockfishCommand();
    execlp(cmd.c_str(), cmd.c_str(), (char *)NULL);

    _exit(127);
  }

  childPid = pid;
  close(childReadPipe);
  close(childWritePipe);

  parentReadPipeF = fdopen(parentReadPipe, readMode);
  parentWritePipeF = fdopen(parentWritePipe, writeMode);
  setvbuf(parentWritePipeF, NULL, _IOLBF, 0);
#endif

  std::string line;
  std::vector<std::string> splittedLine;

  line = readLine(parentReadPipeF, true);
  splittedLine = split(line, ' ');
  // Banner is usually "Stockfish <version> by ..."
  if(splittedLine.empty() ||
     (splittedLine.at(0).find("Stockfish") == std::string::npos &&
      splittedLine.at(0).find("stockfish") == std::string::npos))
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
#ifdef _WIN32
  std::cout << "[Stockfish] Engine ready (Windows process)\n";
#else
  std::cout << "[Stockfish] Engine ready (pid=" << childPid << ")\n";
#endif
}

std::string StockfishConnector::getNextAIMoveFromFen(const std::string& fen){
  if(!isAlive()){
    restartCommunication();
  }

  auto askOnce = [&]() -> std::string {
    std::string line;
    std::vector<std::string> splittedLine;

    std::cout << std::endl << "[Stockfish] position fen " << fen << std::endl;

    writeLine(parentWritePipeF, "stop\n", false);

    const int ms = movetimeMsForSkill(difficultyLevel);
    line = "position fen ";
    line.append(fen);
    line.append("\nisready\n");
    writeLine(parentWritePipeF, line, true);

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
    aiMove.erase(std::remove(aiMove.begin(), aiMove.end(), '\r'), aiMove.end());
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
  aiMove.erase(std::remove(aiMove.begin(), aiMove.end(), '\r'), aiMove.end());
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
