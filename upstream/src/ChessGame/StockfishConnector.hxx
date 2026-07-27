#ifndef STOCKFISHCONNECTOR_HXX
#define STOCKFISHCONNECTOR_HXX

#include <iostream>
#include <string>

#include "../constants.hxx"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

class StockfishConnector {
private:
  /* Communication pipes between child and parent processes
    (respectively stockfish and the GUI)
  */
  FILE* parentWritePipeF;
  FILE* parentReadPipeF;

#ifdef _WIN32
  HANDLE childProcess;
  HANDLE childThread;
#else
  int childPid;
#endif

  /* All the moves since the beginning of the game */
  std::string moves;

  /* Game difficulty */
  int difficultyLevel = DIFFICULTY_EASY;

  bool engineAlive;

  void closePipes();
  void killChild();

public:
  /* Constructor */
  StockfishConnector();

  /* Function which starts Stockfish and initialize the communication
    /throw ConnectionException if something went wrong while initializing
      connection with stockfish
  */
  void startCommunication();

  /** Restart a dead/hung engine (safe to call when already running). */
  void restartCommunication();

  bool isAlive() const { return engineAlive && parentReadPipeF && parentWritePipeF; }

  /* Get the next AI move according to the last user move (legacy move-list).
    Prefer getNextAIMoveFromFen() — cumulative UCI lists desync when the GUI
    omits castling / en passant / check rules.
  */
  std::string getNextAIMove(std::string userMove);

  /* Ask Stockfish from an explicit FEN (board is source of truth). */
  std::string getNextAIMoveFromFen(const std::string& fen);

  /* Suggested next user move, "(none)" is nothing is suggested by the AI */
  std::string suggestedUserMove = "(none)";

  /* Destructor, this will properly stop the communication */
  ~StockfishConnector();
};

#endif
