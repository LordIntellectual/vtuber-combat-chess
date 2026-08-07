#include "../Event/Event.hxx"
#include "../Event/EventStack.hxx"

#include "StockfishConnector.hxx"
#include <iostream>
#include "GameException.hxx"

#include "ChessGame.hxx"

#include <cctype>
#include <iostream>


ChessGame::ChessGame(){
  // Start communication with stockfish
  stockfishConnector = new StockfishConnector();

  lastUserMove = "";
  clock = new Clock();
};

void ChessGame::start(){
  stockfishConnector->startCommunication();
}

Vector2i ChessGame::uciFormatToPosition(std::string position){
  int x(0), y(0);
  bool found(false);
  for(x = 0; x < 8 and not found; x++){
    for(y = 0; y < 8 and not found; y++){
      if(uciGrid[x][y].compare(position) == 0){
        found = true;
      }
    }
  }

  if(not found) throw GameException("Oups, something went wrong...");

  Vector2i outPosition = {x - 1, y - 1};
  return outPosition;
};

std::string ChessGame::positionToUciFormat(Vector2i position){
  if(position.x < 0 || position.x > 7 || position.y < 0 || position.y > 7)
    throw GameException("Oups, something went wrong...");

  return uciGrid[position.x][position.y];
};

std::string ChessGame::boardToFen(bool whiteToMove) const {
  std::string fen;
  for(int y = 7; y >= 0; y--){
    int empty = 0;
    for(int x = 0; x < 8; x++){
      int p = board[x][y];
      if(p == EMPTY){
        empty++;
        continue;
      }
      if(empty > 0){
        fen.push_back(static_cast<char>('0' + empty));
        empty = 0;
      }
      char c = 'p';
      switch(abs(p)){
        case KING: c = 'k'; break;
        case QUEEN: c = 'q'; break;
        case ROOK: c = 'r'; break;
        case BISHOP: c = 'b'; break;
        case KNIGHT: c = 'n'; break;
        case PAWN: c = 'p'; break;
        default: c = 'p'; break;
      }
      if(p > 0) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
      fen.push_back(c);
    }
    if(empty > 0) fen.push_back(static_cast<char>('0' + empty));
    if(y > 0) fen.push_back('/');
  }
  // No castling / en-passant tracking in the GUI yet — omit rights so engine
  // never proposes moves we cannot apply correctly.
  fen += whiteToMove ? " w " : " b ";
  fen += "- - 0 1";
  return fen;
}

void ChessGame::applyMoveSideEffects(int piece, Vector2i start, Vector2i end){
  // Castling: king jumps two files — slide rook to the other side of the king
  if(abs(piece) == KING && abs(end.x - start.x) == 2){
    int y = start.y;
    if(end.x > start.x){
      // kingside: h-rook → f
      int rook = board[7][y];
      board[7][y] = EMPTY;
      board[end.x - 1][y] = rook;
    } else {
      // queenside: a-rook → d
      int rook = board[0][y];
      board[0][y] = EMPTY;
      board[end.x + 1][y] = rook;
    }
  }

  // En passant: pawn captures diagonally onto empty square — remove the
  // passed pawn that sits on the start rank of the capturer.
  if(abs(piece) == PAWN && start.x != end.x &&
     boardAt(end.x, end.y) == EMPTY){
    board[end.x][start.y] = EMPTY;
  }
}

const int ChessGame::boardAt(int x, int y) const {
  if(0 <= x and x < 8 and 0 <= y and y < 8){
    return board[x][y];
  }else{
    return OUT_OF_BOUND;
  }
};

// White pawns promote on y==7 (rank 8); black on y==0 (rank 1). Default: queen.
static int pieceAfterPromotion(int piece, Vector2i endPos){
  if(abs(piece) != PAWN) return piece;
  if(piece > 0 && endPos.y == 7) return QUEEN;
  if(piece < 0 && endPos.y == 0) return -QUEEN;
  return piece;
}

static char uciPromotionChar(int placedPiece){
  switch(abs(placedPiece)){
    case QUEEN: return 'q';
    case ROOK: return 'r';
    case BISHOP: return 'b';
    case KNIGHT: return 'n';
    default: return 0;
  }
}

static int pieceFromUciPromotion(int movingSidePiece, char promo){
  int side = movingSidePiece > 0 ? 1 : -1;
  switch(promo){
    case 'q': case 'Q': return side * QUEEN;
    case 'r': case 'R': return side * ROOK;
    case 'b': case 'B': return side * BISHOP;
    case 'n': case 'N': return side * KNIGHT;
    default: return movingSidePiece;
  }
}

// Mark a destination only when it is a real board square (prevents OOB writes).
// Critical: boardAt() returns OUT_OF_BOUND (==8) off-board; that must NEVER be
// treated as an enemy piece (e.g. black capture tests used `> 0`, and 8 > 0).
static inline bool onBoard(int x, int y) {
  return x >= 0 && x < 8 && y >= 0 && y < 8;
}
static inline void allowAt(bool allowed[8][8], int x, int y) {
  if(onBoard(x, y)) allowed[x][y] = true;
}

void ChessGame::computePAWNNextPositions(Vector2i position){
  // Moving one tile
  if(onBoard(position.x, position.y + 1) &&
     boardAt(position.x, position.y + 1) == EMPTY)
    allowAt(allowedNextPositions, position.x, position.y + 1);

  // Moving two tiles
  if(position.y == 1 and
      boardAt(position.x, position.y + 1) == EMPTY and
      boardAt(position.x, position.y + 2) == EMPTY)
    allowAt(allowedNextPositions, position.x, position.y + 2);

  // Moving in diagonal (capture black pieces only — must be on-board)
  if(onBoard(position.x - 1, position.y + 1) &&
     boardAt(position.x - 1, position.y + 1) < 0)
    allowAt(allowedNextPositions, position.x - 1, position.y + 1);
  if(onBoard(position.x + 1, position.y + 1) &&
     boardAt(position.x + 1, position.y + 1) < 0)
    allowAt(allowedNextPositions, position.x + 1, position.y + 1);
};

void ChessGame::computeROOKNextPositions(Vector2i position){
  int dx, dy;
  // Moving forward
  for(dy = 1; position.y + dy < 8; dy++){
    if(boardAt(position.x, position.y + dy) <= 0)
      allowedNextPositions[position.x][position.y + dy] = true;
    if(boardAt(position.x, position.y + dy) != EMPTY)
      break;
  }

  // Moving backward
  for(dy = 1; position.y - dy >= 0; dy++){
    if(boardAt(position.x, position.y - dy) <= 0)
      allowedNextPositions[position.x][position.y - dy] = true;
    if(boardAt(position.x, position.y - dy) != EMPTY)
      break;
  }

  // Moving to the right
  for(dx = 1; position.x + dx < 8; dx++){
    if(boardAt(position.x + dx, position.y) <= 0)
      allowedNextPositions[position.x + dx][position.y] = true;
    if(boardAt(position.x + dx, position.y) != EMPTY)
      break;
  }

  // Moving to the left
  for(dx = 1; position.x - dx >= 0; dx++){
    if(boardAt(position.x - dx, position.y) <= 0)
      allowedNextPositions[position.x - dx][position.y] = true;
    if(boardAt(position.x - dx, position.y) != EMPTY)
      break;
  }
};

void ChessGame::computeKNIGHTNextPositions(Vector2i position){
  const int d[8][2] = {
    {1, 2}, {2, 1}, {-1, 2}, {-2, 1}, {1, -2}, {2, -1}, {-1, -2}, {-2, -1}
  };
  for(int i = 0; i < 8; i++){
    int nx = position.x + d[i][0];
    int ny = position.y + d[i][1];
    if(!onBoard(nx, ny)) continue;
    // Empty or enemy (white-centric: dest <= 0)
    if(boardAt(nx, ny) <= 0)
      allowAt(allowedNextPositions, nx, ny);
  }
};

void ChessGame::computeBISHOPNextPositions(Vector2i position){
  int dd;
  // Moving forward/right
  for(dd = 1; position.x + dd < 8 and position.y + dd < 8; dd++){
    if(boardAt(position.x + dd, position.y + dd) <= 0)
      allowedNextPositions[position.x + dd][position.y + dd] = true;
    if(boardAt(position.x + dd, position.y + dd) != EMPTY)
      break;
  }

  // Moving forward/left
  for(dd = 1; position.x - dd >= 0 and position.y + dd < 8; dd++){
    if(boardAt(position.x - dd, position.y + dd) <= 0)
      allowedNextPositions[position.x - dd][position.y + dd] = true;
    if(boardAt(position.x - dd, position.y + dd) != EMPTY)
      break;
  }

  // Moving backward/right
  for(dd = 1; position.x + dd < 8 and position.y - dd < 8; dd++){
    if(boardAt(position.x + dd, position.y - dd) <= 0)
      allowedNextPositions[position.x + dd][position.y - dd] = true;
    if(boardAt(position.x + dd, position.y - dd) != EMPTY)
      break;
  }

  // Moving backward/left
  for(dd = 1; position.x - dd >= 0 and position.y - dd < 8; dd++){
    if(boardAt(position.x - dd, position.y - dd) <= 0)
      allowedNextPositions[position.x - dd][position.y - dd] = true;
    if(boardAt(position.x - dd, position.y - dd) != EMPTY)
      break;
  }
};

void ChessGame::computeKINGNextPositions(Vector2i position){
  for(int dx = -1; dx <= 1; dx++){
    for(int dy = -1; dy <= 1; dy++){
      if(dx == 0 && dy == 0) continue;
      int nx = position.x + dx;
      int ny = position.y + dy;
      if(!onBoard(nx, ny)) continue;
      if(boardAt(nx, ny) <= 0)
        allowAt(allowedNextPositions, nx, ny);
    }
  }
};

bool ChessGame::isSquareAttacked(int x, int y, bool byWhite) const {
  if(x < 0 || x > 7 || y < 0 || y > 7) return false;

  // Pawns
  if(byWhite){
    // White pawns attack up-diagonals (toward +y)
    if(boardAt(x - 1, y - 1) == PAWN) return true;
    if(boardAt(x + 1, y - 1) == PAWN) return true;
  } else {
    if(boardAt(x - 1, y + 1) == -PAWN) return true;
    if(boardAt(x + 1, y + 1) == -PAWN) return true;
  }

  // Knights
  const int kn[8][2] = {
    {1, 2}, {2, 1}, {-1, 2}, {-2, 1}, {1, -2}, {2, -1}, {-1, -2}, {-2, -1}
  };
  int wantKnight = byWhite ? KNIGHT : -KNIGHT;
  for(int i = 0; i < 8; i++){
    if(boardAt(x + kn[i][0], y + kn[i][1]) == wantKnight) return true;
  }

  // King (adjacent)
  int wantKing = byWhite ? KING : -KING;
  for(int dx = -1; dx <= 1; dx++)
    for(int dy = -1; dy <= 1; dy++){
      if(dx == 0 && dy == 0) continue;
      if(boardAt(x + dx, y + dy) == wantKing) return true;
    }

  // Sliding: rook/queen orthogonal
  const int orth[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
  for(int d = 0; d < 4; d++){
    int cx = x + orth[d][0], cy = y + orth[d][1];
    while(cx >= 0 && cx < 8 && cy >= 0 && cy < 8){
      int p = board[cx][cy];
      if(p != EMPTY){
        int ap = abs(p);
        bool whitePiece = p > 0;
        if(whitePiece == byWhite && (ap == ROOK || ap == QUEEN)) return true;
        break;
      }
      cx += orth[d][0];
      cy += orth[d][1];
    }
  }

  // Sliding: bishop/queen diagonal
  const int diag[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
  for(int d = 0; d < 4; d++){
    int cx = x + diag[d][0], cy = y + diag[d][1];
    while(cx >= 0 && cx < 8 && cy >= 0 && cy < 8){
      int p = board[cx][cy];
      if(p != EMPTY){
        int ap = abs(p);
        bool whitePiece = p > 0;
        if(whitePiece == byWhite && (ap == BISHOP || ap == QUEEN)) return true;
        break;
      }
      cx += diag[d][0];
      cy += diag[d][1];
    }
  }
  return false;
}

bool ChessGame::isKingInCheck(bool whiteKing) const {
  int want = whiteKing ? KING : -KING;
  for(int x = 0; x < 8; x++)
    for(int y = 0; y < 8; y++)
      if(board[x][y] == want)
        return isSquareAttacked(x, y, !whiteKing);
  // No king (should not happen) — treat as not in check
  return false;
}

bool ChessGame::isHighlightPulseSquare(int x, int y) const {
  if(x < 0 || x > 7 || y < 0 || y > 7) return false;

  // Engine-suggested move tiles (caller may clear these when setting is off)
  if((suggestedUserMoveStartPosition.x == x &&
      suggestedUserMoveStartPosition.y == y) ||
     (suggestedUserMoveEndPosition.x == x &&
      suggestedUserMoveEndPosition.y == y))
    return true;

  // Checked king always pulses with the same motion so players notice check
  // even when Suggested Moves is disabled.
  int piece = board[x][y];
  if(piece == KING) return isKingInCheck(true);
  if(piece == -KING) return isKingInCheck(false);
  return false;
}

void ChessGame::filterMovesLeavingKingInCheck(Vector2i from, int piece){
  const bool whiteMover = piece > 0;
  for(int x = 0; x < 8; x++){
    for(int y = 0; y < 8; y++){
      if(!allowedNextPositions[x][y]) continue;

      // Simulate move (including en passant capture)
      int captured = board[x][y];
      int epCaptured = EMPTY;
      Vector2i epPos = {-1, -1};
      board[x][y] = piece;
      board[from.x][from.y] = EMPTY;
      if(abs(piece) == PAWN && from.x != x && captured == EMPTY){
        // En passant: victim sits on start rank of capturer
        epPos = {x, from.y};
        epCaptured = board[epPos.x][epPos.y];
        board[epPos.x][epPos.y] = EMPTY;
      }

      bool leavesInCheck = isKingInCheck(whiteMover);

      // Undo
      board[from.x][from.y] = piece;
      board[x][y] = captured;
      if(epPos.x >= 0) board[epPos.x][epPos.y] = epCaptured;

      if(leavesInCheck) allowedNextPositions[x][y] = false;
    }
  }
}

void ChessGame::computeAllowedNextPositions(){
  // Reset matrix
  resetAllowedNextPositions();

  // Get the selected piece
  Vector2i piecePosition = selectedPiecePosition;
  const int piece = boardAt(piecePosition.x, piecePosition.y);

  // Nothing selected or empty
  if((piecePosition.x == -1 and piecePosition.y == -1) or piece == EMPTY){
    return;
  }

  // White (user) pieces only on USER_TURN; black only on BLACK_TURN
  if(state == USER_TURN && piece < 0) return;
  if(state == BLACK_TURN && piece > 0) return;
  if(state != USER_TURN && state != BLACK_TURN) return;

  // Piece-type rules (use absolute type; PAWN dir depends on side)
  int ptype = abs(piece);
  if(ptype == PAWN){
    if(piece > 0){
      computePAWNNextPositions(piecePosition);
    } else {
      // Black pawn moves toward decreasing y
      int x = piecePosition.x;
      int y = piecePosition.y;
      if(onBoard(x, y - 1) && boardAt(x, y - 1) == EMPTY)
        allowAt(allowedNextPositions, x, y - 1);
      if(y == 6 &&
          boardAt(x, y - 1) == EMPTY &&
          boardAt(x, y - 2) == EMPTY)
        allowAt(allowedNextPositions, x, y - 2);
      // Capture white only. Must be on-board: boardAt off-board is OUT_OF_BOUND
      // (==8) which is > 0 and must not be treated as a capturable piece.
      if(onBoard(x - 1, y - 1) && boardAt(x - 1, y - 1) > 0 &&
         boardAt(x - 1, y - 1) != OUT_OF_BOUND)
        allowAt(allowedNextPositions, x - 1, y - 1);
      if(onBoard(x + 1, y - 1) && boardAt(x + 1, y - 1) > 0 &&
         boardAt(x + 1, y - 1) != OUT_OF_BOUND)
        allowAt(allowedNextPositions, x + 1, y - 1);
    }
    // Legal only if own king is not left in check
    filterMovesLeavingKingInCheck(piecePosition, piece);
    return;
  }

  // White-centric generators treat dest<=0 as legal. For black, temporarily
  // invert board signs so the same generators produce correct captures.
  int saved[8][8];
  bool invert = (piece < 0);
  if(invert){
    for(int x = 0; x < 8; x++)
      for(int y = 0; y < 8; y++){
        saved[x][y] = board[x][y];
        board[x][y] = -board[x][y];
      }
  }

  switch (ptype) {
    case ROOK:
      computeROOKNextPositions(piecePosition);
      break;
    case KNIGHT:
      computeKNIGHTNextPositions(piecePosition);
      break;
    case BISHOP:
      computeBISHOPNextPositions(piecePosition);
      break;
    case QUEEN:
      computeBISHOPNextPositions(piecePosition);
      computeROOKNextPositions(piecePosition);
      break;
    case KING:
      computeKINGNextPositions(piecePosition);
      break;
  }

  if(invert){
    for(int x = 0; x < 8; x++)
      for(int y = 0; y < 8; y++)
        board[x][y] = saved[x][y];
  }

  // Drop moves that leave the mover's king in check (was causing illegal FENs
  // that crash Stockfish and kill the AI for the rest of the session).
  filterMovesLeavingKingInCheck(piecePosition, piece);
};

void ChessGame::resetAllowedNextPositions(){
  for(int x = 0; x < 8; x++)
    for(int y = 0; y < 8; y++)
      allowedNextPositions[x][y] = false;
};

bool ChessGame::onlyKingRemains(bool whiteSide) const {
  bool hasKing = false;
  for(int x = 0; x < 8; x++){
    for(int y = 0; y < 8; y++){
      int p = board[x][y];
      if(p == EMPTY) continue;
      bool isWhite = p > 0;
      if(isWhite != whiteSide) continue;
      if(abs(p) == KING) hasKing = true;
      else return false; // has a non-king piece
    }
  }
  return hasKing;
}

bool ChessGame::sideHasLegalMove(bool whiteSide){
  Vector2i savedSel = selectedPiecePosition;
  Vector2i savedOld = oldSelectedPiecePosition;
  int savedState = state;
  bool any = false;

  // computeAllowedNextPositions gates on state
  state = whiteSide ? USER_TURN : BLACK_TURN;

  for(int x = 0; x < 8 && !any; x++){
    for(int y = 0; y < 8 && !any; y++){
      int p = board[x][y];
      if(p == EMPTY) continue;
      if(whiteSide && p < 0) continue;
      if(!whiteSide && p > 0) continue;
      selectedPiecePosition = {x, y};
      computeAllowedNextPositions();
      for(int i = 0; i < 8 && !any; i++)
        for(int j = 0; j < 8 && !any; j++)
          if(allowedNextPositions[i][j]) any = true;
    }
  }

  selectedPiecePosition = savedSel;
  oldSelectedPiecePosition = savedOld;
  state = savedState;
  resetAllowedNextPositions();
  return any;
}

bool ChessGame::evaluateEndAfterMove(bool whiteJustMoved){
  // Defender is the side that must now move
  bool defenderWhite = !whiteJustMoved;
  bool inCheck = isKingInCheck(defenderWhite);
  bool hasMove = sideHasLegalMove(defenderWhite);
  bool onlyKing = onlyKingRemains(defenderWhite);

  if(inCheck && !hasMove){
    endReason = END_CHECKMATE;
    whiteWon = !defenderWhite;
    state = GAME_OVER;
    victoryFxPending = true;
    std::cout << "[Chess] CHECKMATE — "
              << (whiteWon ? "White" : "Black") << " wins" << std::endl;
    return true;
  }
  if(onlyKing){
    // Every enemy piece except the king has been eliminated
    endReason = END_FORFEIT;
    whiteWon = !defenderWhite;
    state = GAME_OVER;
    victoryFxPending = true;
    std::cout << "[Chess] FORFEIT — "
              << (whiteWon ? "White" : "Black")
              << " wins (opponent has only king)" << std::endl;
    return true;
  }
  return false;
}

void ChessGame::setAiEnabled(bool enabled){
  // Network session owns AI off — ignore toggles that would re-enable
  if(netRole != NET_NONE && enabled){
    aiEnabled = false;
    return;
  }
  aiEnabled = enabled;
  // If we were waiting for AI and AI was just disabled, give black to human.
  if(!aiEnabled && (state == AI_TURN || state == WAITING)){
    state = BLACK_TURN;
    selectedPiecePosition = {-1, -1};
    oldSelectedPiecePosition = {-1, -1};
    resetAllowedNextPositions();
  }
}

void ChessGame::setNetworkRole(NetRole role, int localSideSign){
  if(role == NET_NONE){
    clearNetworkRole();
    return;
  }
  if(netRole == NET_NONE)
    aiEnabledBeforeNet = aiEnabled;
  netRole = role;
  netLocalSide = (localSideSign >= 0) ? 1 : -1;
  aiEnabled = false;
  if(state == AI_TURN || state == WAITING)
    state = BLACK_TURN;
  pendingMoveReq.clear();
  localMoveBroadcast.clear();
  selectedPiecePosition = {-1, -1};
  oldSelectedPiecePosition = {-1, -1};
  resetAllowedNextPositions();
}

void ChessGame::clearNetworkRole(){
  if(netRole == NET_NONE) return;
  netRole = NET_NONE;
  netLocalSide = 1;
  pendingMoveReq.clear();
  localMoveBroadcast.clear();
  setAiEnabled(aiEnabledBeforeNet);
}

bool ChessGame::canLocalPlayerMove() const {
  if(state != USER_TURN && state != BLACK_TURN) return false;
  if(netRole == NET_NONE) return true;
  bool whiteTurn = (state == USER_TURN);
  bool localWhite = (netLocalSide > 0);
  return whiteTurn == localWhite;
}

char ChessGame::sideToMoveChar() const {
  if(state == USER_TURN || state == USER_MOVING) return 'w';
  if(state == BLACK_TURN || state == BLACK_MOVING ||
     state == AI_TURN || state == AI_MOVING || state == WAITING)
    return 'b';
  return 'w';
}

std::string ChessGame::getFen() const {
  return boardToFen(sideToMoveChar() == 'w');
}

std::string ChessGame::consumePendingMoveReq(){
  std::string s = pendingMoveReq;
  pendingMoveReq.clear();
  return s;
}

std::string ChessGame::consumeLocalMoveBroadcast(){
  std::string s = localMoveBroadcast;
  localMoveBroadcast.clear();
  return s;
}

bool ChessGame::consumeAnimationJustStarted(){
  bool v = animationJustStarted_;
  animationJustStarted_ = false;
  return v;
}

bool ChessGame::beginAnimatedMove(Vector2i start, Vector2i end, int placedPiece,
                                  bool fromWhiteTurn){
  int piece = boardAt(start.x, start.y);
  if(piece == EMPTY || piece == OUT_OF_BOUND) return false;

  movingPiece = piece;
  movingPieceStartPosition = start;
  movingPieceEndPosition = end;
  movingPiecePosition = {(float)start.x, (float)start.y};
  movingPiecePlaced = (placedPiece != EMPTY) ? placedPiece
                      : pieceAfterPromotion(piece, end);

  {
    int dest = boardAt(end.x, end.y);
    movingIsCapture = (dest != EMPTY && dest * piece < 0);
    if(!movingIsCapture && abs(piece) == PAWN && start.x != end.x){
      int ep = boardAt(end.x, start.y);
      movingIsCapture = (ep != EMPTY && ep * piece < 0);
    }
  }

  applyMoveSideEffects(piece, start, end);
  board[start.x][start.y] = EMPTY;

  lastUserMove.clear();
  lastUserMove.append(positionToUciFormat(start));
  lastUserMove.append(positionToUciFormat(end));
  if(char promo = uciPromotionChar(movingPiecePlaced)){
    if(abs(piece) == PAWN && movingPiecePlaced != piece)
      lastUserMove.push_back(promo);
  }

  oldSelectedPiecePosition = {-1, -1};
  selectedPiecePosition = {-1, -1};
  suggestedUserMoveStartPosition = {-1, -1};
  suggestedUserMoveEndPosition = {-1, -1};
  resetAllowedNextPositions();

  state = fromWhiteTurn ? USER_MOVING : BLACK_MOVING;
  clock->restart();
  movePly += 1;
  animationJustStarted_ = true;
  return true;
}

bool ChessGame::tryApplyUciMove(const std::string& uci, bool trusted){
  if(state == GAME_OVER) return false;
  if(state != USER_TURN && state != BLACK_TURN) return false;
  if(uci.size() < 4) return false;

  Vector2i start, end;
  try {
    start = uciFormatToPosition(uci.substr(0, 2));
    end = uciFormatToPosition(uci.substr(2, 2));
  } catch (...) {
    return false;
  }

  int piece = boardAt(start.x, start.y);
  if(piece == EMPTY || piece == OUT_OF_BOUND) return false;

  bool whiteTurn = (state == USER_TURN);
  if(whiteTurn && piece < 0) return false;
  if(!whiteTurn && piece > 0) return false;

  int placed = EMPTY;
  if(uci.size() >= 5 && std::isalpha(static_cast<unsigned char>(uci[4]))){
    placed = pieceFromUciPromotion(piece, uci[4]);
  } else {
    placed = pieceAfterPromotion(piece, end);
  }

  if(!trusted){
    // Validate via allowed-move matrix for this piece
    Vector2i savedSel = selectedPiecePosition;
    selectedPiecePosition = start;
    computeAllowedNextPositions();
    bool ok = allowedNextPositions[end.x][end.y];
    selectedPiecePosition = savedSel;
    resetAllowedNextPositions();
    if(!ok) return false;
  }

  return beginAnimatedMove(start, end, placed, whiteTurn);
}

void ChessGame::resetBoard(){
  int opening[8][8] = {
    {ROOK, PAWN, EMPTY, EMPTY, EMPTY, EMPTY, AI*PAWN, AI*ROOK},
    {KNIGHT, PAWN, EMPTY, EMPTY, EMPTY, EMPTY, AI*PAWN, AI*KNIGHT},
    {BISHOP, PAWN, EMPTY, EMPTY, EMPTY, EMPTY, AI*PAWN, AI*BISHOP},
    {QUEEN, PAWN, EMPTY, EMPTY, EMPTY, EMPTY, AI*PAWN, AI*QUEEN},
    {KING, PAWN, EMPTY, EMPTY, EMPTY, EMPTY, AI*PAWN, AI*KING},
    {BISHOP, PAWN, EMPTY, EMPTY, EMPTY, EMPTY, AI*PAWN, AI*BISHOP},
    {KNIGHT, PAWN, EMPTY, EMPTY, EMPTY, EMPTY, AI*PAWN, AI*KNIGHT},
    {ROOK, PAWN, EMPTY, EMPTY, EMPTY, EMPTY, AI*PAWN, AI*ROOK}
  };
  for(int x = 0; x < 8; x++)
    for(int y = 0; y < 8; y++)
      board[x][y] = opening[x][y];

  state = USER_TURN;
  endReason = END_NONE;
  whiteWon = true;
  victoryFxPending = false;
  movePly = 0;
  pendingMoveReq.clear();
  localMoveBroadcast.clear();
  animationJustStarted_ = false;
  movingPiece = EMPTY;
  movingPiecePlaced = EMPTY;
  movingPiecePosition = {-1, -1};
  movingPieceStartPosition = {-1, -1};
  movingPieceEndPosition = {-1, -1};
  movingIsCapture = false;
  selectedPiecePosition = {-1, -1};
  oldSelectedPiecePosition = {-1, -1};
  suggestedUserMoveStartPosition = {-1, -1};
  suggestedUserMoveEndPosition = {-1, -1};
  lastUserMove = "";
  resetAllowedNextPositions();
  // Fresh move list for Stockfish on next AI request by recreating connector moves:
  // simplest: delete and new connector only if needed — for v1 restart process via new game
  // Keep stockfish session; clear its move list by reconstructing connector.
  if(stockfishConnector){
    delete stockfishConnector;
    stockfishConnector = new StockfishConnector();
    try { stockfishConnector->startCommunication(); } catch(...) {
      // If stockfish fails mid-reset, AI will error on next move
    }
  }
  clock->restart();
}

void ChessGame::setNewSelectedPiecePosition(
    Vector2i newSelectedPiecePosition){
  if(state == USER_TURN || state == BLACK_TURN){
    // Network: only interact on our turn
    if(netRole != NET_NONE && !canLocalPlayerMove())
      return;

    // Register last user clicked position
    oldSelectedPiecePosition = selectedPiecePosition;

    // And set the new selected piece position
    selectedPiecePosition = newSelectedPiecePosition;

    // If the new selected piece is an allowed move, it surely means that the user
    // wants to move a piece: it will be performed at the next "perform" method
    // call
    if(selectedPiecePosition.x >= 0 && selectedPiecePosition.y >= 0 &&
        allowedNextPositions[selectedPiecePosition.x]
                           [selectedPiecePosition.y] == true){
      return;
    }

    // In other cases, compute the new allowedNextPositions matrix
    computeAllowedNextPositions();
  }
};

void ChessGame::perform(){
  if(state == GAME_OVER) return;

  if(state == USER_TURN || state == BLACK_TURN) {
    // If the selected position is an allowed move, it surely means that the
    // user wants to move a piece
    if(selectedPiecePosition.x != -1 and selectedPiecePosition.y != -1 and
        allowedNextPositions[selectedPiecePosition.x]
                            [selectedPiecePosition.y] == true){
      // Client must not apply locally — queue MOVE_REQ for host authority
      if(netRole == NET_CLIENT){
        try {
          pendingMoveReq.clear();
          pendingMoveReq.append(positionToUciFormat(oldSelectedPiecePosition));
          pendingMoveReq.append(positionToUciFormat(selectedPiecePosition));
          int piece = boardAt(oldSelectedPiecePosition.x,
                              oldSelectedPiecePosition.y);
          int placed = pieceAfterPromotion(piece, selectedPiecePosition);
          if(char promo = uciPromotionChar(placed)){
            if(abs(piece) == PAWN && placed != piece)
              pendingMoveReq.push_back(promo);
          }
        } catch (...) {
          pendingMoveReq.clear();
        }
        oldSelectedPiecePosition = {-1, -1};
        selectedPiecePosition = {-1, -1};
        resetAllowedNextPositions();
        return;
      }

      bool fromWhite = (state == USER_TURN);
      Vector2i start = oldSelectedPiecePosition;
      Vector2i end = selectedPiecePosition;
      if(!beginAnimatedMove(start, end, EMPTY, fromWhite))
        return;

      // Host: broadcast this local move to guest
      if(netRole == NET_HOST)
        localMoveBroadcast = lastUserMove;
    }
  }
  else if(state == USER_MOVING or state == AI_MOVING or state == BLACK_MOVING) {
    // If a piece has been taken, remove it from the board and send event
    if(boardAt(movingPieceEndPosition.x, movingPieceEndPosition.y) != EMPTY and
       sqrt(pow((movingPieceEndPosition.x - movingPiecePosition.x) * 4, 2) +
            pow((movingPieceEndPosition.y - movingPiecePosition.y) * 4, 2)) < 3.2){
      Event event;
      event.type = Event::PieceTakenEvent;
      event.piece.position = movingPieceEndPosition;
      event.piece.piece = boardAt(
        movingPieceEndPosition.x, movingPieceEndPosition.y);
      EventStack::pushEvent(event);

      board[movingPieceEndPosition.x][movingPieceEndPosition.y] = EMPTY;
    }

    float elapsedTime = clock->getElapsedTime();
    if(elapsedTime < 1.0){
      movingPiecePosition = {
        elapsedTime * movingPieceEndPosition.x + (1 - elapsedTime) * movingPieceStartPosition.x,
        elapsedTime * movingPieceEndPosition.y + (1 - elapsedTime) * movingPieceStartPosition.y
      };

      // Seng moving piece event
      Event event;
      event.type = Event::PieceMovingEvent;
      event.movingPiece.currentPosition = movingPiecePosition;
      event.movingPiece.startPosition = movingPieceStartPosition;
      event.movingPiece.endPosition = movingPieceEndPosition;
      EventStack::pushEvent(event);
    } else {
      // Land piece (promoted queen if pawn reached last rank)
      int landed = (movingPiecePlaced != EMPTY) ? movingPiecePlaced : movingPiece;
      board[movingPieceEndPosition.x][movingPieceEndPosition.y] = landed;

      // Seng piece stops event
      Event event;
      event.type = Event::PieceStopsEvent;
      event.movingPiece.currentPosition = movingPiecePosition;
      event.movingPiece.startPosition = movingPieceStartPosition;
      event.movingPiece.endPosition = movingPieceEndPosition;
      EventStack::pushEvent(event);

      // Reset attributes
      bool whiteJustMoved = (state == USER_MOVING);
      movingPiece = EMPTY;
      movingPiecePlaced = EMPTY;
      movingPiecePosition = {-1, -1};
      movingPieceStartPosition = {-1, -1};
      movingPieceEndPosition = {-1, -1};
      movingIsCapture = false;

      // Victory: checkmate or forfeit (only king left for defender)
      if(evaluateEndAfterMove(whiteJustMoved)){
        clock->restart();
        return;
      }

      // After white moves → wait for AI or human black; after black/AI → white
      if(whiteJustMoved){
        state = aiEnabled ? WAITING : BLACK_TURN;
      } else {
        // AI_MOVING or BLACK_MOVING finished
        state = USER_TURN;
      }
      clock->restart();
    }
  }
  else if(state == WAITING) {
    if(clock->getElapsedTime() >= 0.35){
      // Double-check end before AI (covers races / stockfish none)
      if(evaluateEndAfterMove(/*whiteJustMoved=*/true)){
        clock->restart();
        return;
      }
      state = aiEnabled ? AI_TURN : BLACK_TURN;
      clock->restart();
    }
  }
  else if(state == AI_TURN) {
    if(!aiEnabled){
      state = BLACK_TURN;
      return;
    }

    // End-game before engine: white delivered checkmate / forfeit last turn
    if(evaluateEndAfterMove(/*whiteJustMoved=*/true)){
      clock->restart();
      return;
    }

    // Never send illegal FENs (white king still in check after white moved)
    if(isKingInCheck(/*whiteKing=*/true)){
      std::cerr << "[Chess] Illegal board: white king still in check at black's "
                   "turn — skipping AI (fix move validation)\n";
      state = USER_TURN;
      clock->restart();
      throw GameException(
        "Illegal position (king in check) — your turn continues");
    }

    std::string fen = boardToFen(/*whiteToMove=*/false);
    std::string aiMove;
    try {
      aiMove = stockfishConnector->getNextAIMoveFromFen(fen);
    } catch (const std::exception& e) {
      // Stockfish returns no move on checkmate/stalemate — re-check locally
      if(evaluateEndAfterMove(/*whiteJustMoved=*/true)){
        clock->restart();
        return;
      }
      std::cerr << "[Chess] Stockfish failed: " << e.what()
                << " — skipping AI turn\n";
      state = USER_TURN;
      clock->restart();
      throw GameException(
        std::string("AI skipped (") + e.what() + ") — your turn");
    }

    if(aiMove.size() < 4){
      if(evaluateEndAfterMove(/*whiteJustMoved=*/true)){
        clock->restart();
        return;
      }
      std::cerr << "[Chess] Incomplete AI move '" << aiMove
                << "' — skipping AI turn\n";
      state = USER_TURN;
      clock->restart();
      throw GameException("AI skipped (no move) — your turn");
    }

    Vector2i aiMoveStartPosition = uciFormatToPosition(aiMove.substr(0, 2));
    Vector2i aiMoveEndPosition = uciFormatToPosition(aiMove.substr(2, 2));
    int pieceOnStart = boardAt(aiMoveStartPosition.x, aiMoveStartPosition.y);

    // Must be a black piece on the start square
    if(pieceOnStart >= 0){
      std::cerr << "[Chess] AI move " << aiMove
                << " does not match board (start piece=" << pieceOnStart
                << ", fen=" << fen << ") — recovering, white to move\n";
      // Soft recover without killing AI for the rest of the session
      state = USER_TURN;
      clock->restart();
      throw GameException(
        "AI move skipped (sync) — your turn continues");
    }

    // Set the currently moving piece
    movingPiece = pieceOnStart;
    movingPieceStartPosition = aiMoveStartPosition;
    movingPieceEndPosition = aiMoveEndPosition;
    movingPiecePosition = {
      (float)aiMoveStartPosition.x, (float)aiMoveStartPosition.y
    };
    // AI promotion: UCI is 5 chars e.g. e2e1q
    if(aiMove.size() >= 5 && std::isalpha(static_cast<unsigned char>(aiMove[4]))){
      movingPiecePlaced = pieceFromUciPromotion(movingPiece, aiMove[4]);
    } else {
      movingPiecePlaced = pieceAfterPromotion(
        movingPiece, movingPieceEndPosition);
    }

    // Capture? Check before side-effects clear en passant victims
    {
      int dest = boardAt(movingPieceEndPosition.x, movingPieceEndPosition.y);
      movingIsCapture = (dest != EMPTY && dest * movingPiece < 0);
      if (!movingIsCapture && abs(movingPiece) == PAWN &&
          movingPieceStartPosition.x != movingPieceEndPosition.x) {
        int ep = boardAt(movingPieceEndPosition.x, movingPieceStartPosition.y);
        movingIsCapture = (ep != EMPTY && ep * movingPiece < 0);
      }
    }

    // Castling / en passant side effects before animation
    applyMoveSideEffects(
      movingPiece, aiMoveStartPosition, aiMoveEndPosition);

    // Remove the piece from its old position
    board[aiMoveStartPosition.x][aiMoveStartPosition.y] = EMPTY;

    // Get suggested user next move if available
    if(stockfishConnector->suggestedUserMove.compare("(none)") != 0 &&
       stockfishConnector->suggestedUserMove.size() >= 4){
      std::string startPosition_str = \
        stockfishConnector->suggestedUserMove.substr(0, 2);
      std::string endPosition_str = \
        stockfishConnector->suggestedUserMove.substr(2, 2);

      suggestedUserMoveStartPosition = uciFormatToPosition(startPosition_str);
      suggestedUserMoveEndPosition = uciFormatToPosition(endPosition_str);
    }else{
      suggestedUserMoveStartPosition = {-1, -1};
      suggestedUserMoveEndPosition = {-1, -1};
    }

    // Transition to AI_MOVING state
    state = AI_MOVING;
    clock->restart();
    animationJustStarted_ = true;
  }
};

ChessGame::~ChessGame(){
  delete stockfishConnector;
  delete clock;
};
