#ifndef CHESSGAME_HXX_
#define CHESSGAME_HXX_

#include <string>
#include <cmath>

#include "../constants.hxx"
#include "../Clock/Clock.hxx"
#include "../utils/math.hxx"
#include "StockfishConnector.hxx"


// cppcheck-suppress noCopyConstructor
class ChessGame {
private:
  /* Last user move */
  std::string lastUserMove;

  /* The connector with Stockfish */
  StockfishConnector* stockfishConnector;

  /* The state of the game, should be USER_TURN, AI_TURN or WAITING */
  int state = USER_TURN;

  /* Clock used for measuring time during piece movement and
  waiting between the USER_TURN and the AI_TURN */
  Clock* clock;

  /* Grid used for conversions between position and UCI format */
  const std::string uciGrid[8][8] = {
    {"a1", "a2", "a3", "a4", "a5", "a6", "a7", "a8"},
    {"b1", "b2", "b3", "b4", "b5", "b6", "b7", "b8"},
    {"c1", "c2", "c3", "c4", "c5", "c6", "c7", "c8"},
    {"d1", "d2", "d3", "d4", "d5", "d6", "d7", "d8"},
    {"e1", "e2", "e3", "e4", "e5", "e6", "e7", "e8"},
    {"f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8"},
    {"g1", "g2", "g3", "g4", "g5", "g6", "g7", "g8"},
    {"h1", "h2", "h3", "h4", "h5", "h6", "h7", "h8"},
  };

  /* Conversion function: converts a position in UCI format (e.g. "h3") into a
  vector
    \param position The position in the UCI format
    \return the vector representing the position on the board
  */
  Vector2i uciFormatToPosition(std::string position);

  /* Conversion function: converts a position specified with x and y into a UCI
  format (e.g. "a5")
    \param position The position as an Vector2i
    \return the position in the UCI format
  */
  std::string positionToUciFormat(Vector2i position);

  /* Build a FEN string from the current board (source of truth for Stockfish). */
  std::string boardToFen(bool whiteToMove) const;

  /* Side effects when a move lands/starts: castling rook, en passant capture */
  void applyMoveSideEffects(int piece, Vector2i start, Vector2i end);

  /* Compute the allowedNextPositions matrix for a specific piece */
  void computePAWNNextPositions(Vector2i position);
  void computeROOKNextPositions(Vector2i position);
  void computeKNIGHTNextPositions(Vector2i position);
  void computeBISHOPNextPositions(Vector2i position);
  void computeKINGNextPositions(Vector2i position);

  /* Compute the allowedNextPositions matrix according to the selected piece */
  void computeAllowedNextPositions();

  /* Reset the allowedNextPositions matrix with its default value */
  void resetAllowedNextPositions();

  /** True if square (x,y) is attacked by white (byWhite=true) or black. */
  bool isSquareAttacked(int x, int y, bool byWhite) const;

  /** True if the given side's king is currently in check. */
  bool isKingInCheck(bool whiteKing) const;

  /**
   * Remove pseudo-legal destinations that leave the mover's king in check.
   * Must be called with the real board (not temporarily inverted).
   */
  void filterMovesLeavingKingInCheck(Vector2i from, int piece);

  /** True if the side has at least one legal move. */
  bool sideHasLegalMove(bool whiteSide);

  /** True if the side has a king and zero other pieces. */
  bool onlyKingRemains(bool whiteSide) const;

  /**
   * After a move by `whiteJustMoved`, check checkmate / forfeit for the
   * defender. Sets GAME_OVER + endReason/whiteWon when finished.
   * \return true if the game ended
   */
  bool evaluateEndAfterMove(bool whiteJustMoved);

public:
  /* Constructor */
  explicit ChessGame();

  /* The checkerboard */
  int board[8][8] = {
    {ROOK, PAWN, EMPTY, EMPTY, EMPTY, EMPTY, AI*PAWN, AI*ROOK},
    {KNIGHT, PAWN, EMPTY, EMPTY, EMPTY, EMPTY, AI*PAWN, AI*KNIGHT},
    {BISHOP, PAWN, EMPTY, EMPTY, EMPTY, EMPTY, AI*PAWN, AI*BISHOP},
    {QUEEN, PAWN, EMPTY, EMPTY, EMPTY, EMPTY, AI*PAWN, AI*QUEEN},
    {KING, PAWN, EMPTY, EMPTY, EMPTY, EMPTY, AI*PAWN, AI*KING},
    {BISHOP, PAWN, EMPTY, EMPTY, EMPTY, EMPTY, AI*PAWN, AI*BISHOP},
    {KNIGHT, PAWN, EMPTY, EMPTY, EMPTY, EMPTY, AI*PAWN, AI*KNIGHT},
    {ROOK, PAWN, EMPTY, EMPTY, EMPTY, EMPTY, AI*PAWN, AI*ROOK}
  };

  /* The allowed next positions for the currently selected piece */
  bool allowedNextPositions[8][8] = {
    {false, false, false, false, false, false, false, false},
    {false, false, false, false, false, false, false, false},
    {false, false, false, false, false, false, false, false},
    {false, false, false, false, false, false, false, false},
    {false, false, false, false, false, false, false, false},
    {false, false, false, false, false, false, false, false},
    {false, false, false, false, false, false, false, false},
    {false, false, false, false, false, false, false, false}
  };

  /* Method used for accessing piece at position {x, y}, if {x, y} doesn't
  correspond to a position on the board, it returns OUT_OF_BOUND constant and
  doesn't throw exception */
  const int boardAt(int x, int y) const;

  /* Position of the currently selected chess piece {-1, -1} if nothing is
  selected */
  Vector2i selectedPiecePosition = {-1, -1};

  /* Position of the selected chess piece just before the currently selected
  one was selected */
  Vector2i oldSelectedPiecePosition = {-1, -1};

  /* Start position of the suggested user move, {-1, -1} if nothing is suggested
  */
  Vector2i suggestedUserMoveStartPosition = {-1, -1};

  /* End position of the suggested user move, {-1, -1} if nothing is suggested
  */
  Vector2i suggestedUserMoveEndPosition = {-1, -1};

  /**
   * True if this board square should bob like a suggested-move highlight:
   * engine suggestion start/end (when those coords are set), and/or a king
   * currently in check. Check pulse is independent of the Suggested Moves
   * setting; if the king already pulses as a suggestion, no extra effect.
   */
  bool isHighlightPulseSquare(int x, int y) const;

  /* Start the game engine
    \throw ConnectionException if communication with Stockfish didn't start
    properly
  */
  void start();

  /* Set the new clicked position on the board */
  void setNewSelectedPiecePosition(Vector2i newSelectedPiecePosition);

  /* Perform the chess rules depending on the game state, if it's the USER_TURN
    it will move one chess piece according to the currently clicked piece, if
    it's WAITING it will wait one second before changing to AI_TURN, if it's
    AI_TURN it will ask Stockfish what is the next AI move
    \throw GameException if chess rules are not respected
  */
  void perform();

  /* Currently moving piece: KING, QUEEN, ... EMPTY if nothing is currently
  moving (piece type while animating — still pawn until promotion lands) */
  int movingPiece = EMPTY;

  /* Piece written when the animation finishes (QUEEN after pawn promotion) */
  int movingPiecePlaced = EMPTY;

  /* Currently moving piece position */
  Vector2f movingPiecePosition = {-1, -1};

  /* The start and end position of the currently moving piece */
  Vector2i movingPieceStartPosition = {-1, -1};
  Vector2i movingPieceEndPosition = {-1, -1};

  /** True while animating a move that captures (normal or en passant). */
  bool movingIsCapture = false;

  /* vTuber Combat Chess: Stockfish AI toggle */
  bool aiEnabled = true;

  /* Current state machine value (USER_TURN, AI_TURN, ...) */
  int getState() const { return state; }

  /* Toggle AI; when disabled, human can play black pieces */
  void setAiEnabled(bool enabled);
  bool isAiEnabled() const { return aiEnabled; }

  /* Reset board to opening position (keeps Stockfish process) */
  void resetBoard();

  /* Last completed move in UCI (for HUD / stream) */
  std::string getLastUserMove() const { return lastUserMove; }

  /** END_NONE / END_CHECKMATE / END_FORFEIT when state == GAME_OVER. */
  int endReason = END_NONE;
  /** True if white won (false = black won). */
  bool whiteWon = true;
  /** Set when entering GAME_OVER so main can fire VFX once. */
  bool victoryFxPending = false;

  bool isGameOver() const { return state == GAME_OVER; }

  /* --- Networked multiplayer (see docs/MULTIPLAYER_DESIGN.md) --- */
  enum NetRole { NET_NONE = 0, NET_HOST = 1, NET_CLIENT = 2 };

  /** Enable network mode. localSideSign: +1 white, -1 black. Forces AI off. */
  void setNetworkRole(NetRole role, int localSideSign);
  void clearNetworkRole();
  NetRole networkRole() const { return netRole; }
  bool isNetworked() const { return netRole != NET_NONE; }
  bool isNetAuthority() const { return netRole != NET_CLIENT; }
  int localSideSign() const { return netLocalSide; }

  /** True if local input may commit a move this turn. */
  bool canLocalPlayerMove() const;

  /**
   * Apply a UCI move and start animation (USER_MOVING / BLACK_MOVING).
   * Host validates legality; client may pass trusted=true for host MOVE.
   * \return true if accepted
   */
  bool tryApplyUciMove(const std::string& uci, bool trusted = false);

  /** FEN for current board; side-to-move derived from state when possible. */
  std::string getFen() const;

  /** Side to move: 'w' or 'b' (best-effort from state). */
  char sideToMoveChar() const;

  /** Half-move / ply counter for network (incremented when a move starts). */
  int movePly = 0;

  /**
   * Client-only: UCI waiting to be sent as MOVE_REQ (set by perform when
   * local player commits a destination). Cleared by consumePendingMoveReq().
   */
  bool hasPendingMoveReq() const { return !pendingMoveReq.empty(); }
  std::string consumePendingMoveReq();

  /**
   * Host: UCI of a move that just started from local input this perform().
   * Cleared by consumeLocalMoveBroadcast().
   */
  bool hasLocalMoveBroadcast() const { return !localMoveBroadcast.empty(); }
  std::string consumeLocalMoveBroadcast();

  /* Destructor */
  ~ChessGame();

private:
  NetRole netRole = NET_NONE;
  int netLocalSide = 1; // +1 white, -1 black
  bool aiEnabledBeforeNet = true;
  std::string pendingMoveReq;
  std::string localMoveBroadcast;

  /** Shared path: begin animated move from start→end (board still has piece). */
  bool beginAnimatedMove(Vector2i start, Vector2i end, int placedPiece,
                         bool fromWhiteTurn);
};

#endif
