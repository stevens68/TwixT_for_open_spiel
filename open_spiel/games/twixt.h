
#ifndef THIRD_PARTY_OPEN_SPIEL_GAMES_TWIXT_H_
#define THIRD_PARTY_OPEN_SPIEL_GAMES_TWIXT_H_


#include <array>
#include <map>
#include <string>
#include <vector>
#include <list>
#include <sstream>
#include <unordered_set>

#include "open_spiel/spiel.h"
#include "open_spiel/games/twixtboard.h"

// https://en.wikipedia.org/wiki/TwixT

namespace open_spiel {
namespace twixt {

const int kNumPlayers = 2;
const int kNumLinkDirections = 8;
const int kMinBoardSize=5;
const int kMaxBoardSize=24;
const int kDefaultBoardSize=8;
const int kMargin = 2;
const int kMaxVirtualBoardSize=kMaxBoardSize+2*kMargin;
const int kDefaultAnsiColorOutput=true;
const int kNumPlanes=11;  // 2 * (1 for pegs + 4 for links) + 1 for player to move
const double kMinDiscount=0.0;
const double kMaxDiscount=1.0;
const double kDefaultDiscount=kMaxDiscount;
const bool kDefaultDrawCheck=false;

// ANSI colors
const std::string kAnsiRed = "\e[91m";
const std::string kAnsiBlue = "\e[94m";
const std::string kAnsiDefault = "\e[0m";

// colors
const int RED = 0;  // player, peg color, board index
const int BLUE = 1; // player, peg color, board index
const int EMPTY = -2; // peg color
const int OVERBOARD = -3; // peg color
const int DEADEND = -4; // peg color, for draw detection only


// eight directions of links from 0 to 7
enum LinkDirections {
  NNE = 0,  // North-North-East, 2 up, 1 right
  ENE = 1,  // East-North-East,  1 up, 2 right
  ESE = 2,  // East-South-East,  1 down, 2 right
  SSE = 3,  // South-South-East, 2 down, 1 right
  SSW = 4,  // South-South-West, 2 down, 1 left
  WSW = 5,  // West-South-West,  1 down, 2 left
  WNW = 6,  // West-North-West,  1 up, 2 left
  NNW = 7,  // North-North-West, 2 up, 1 left
};

const int LINKORDER[kNumPlayers][kNumLinkDirections] = {
		//{4, 3, 5, 2, 6, 1, 7, 0}, {1, 2, 0, 3, 7, 4, 6, 5}
		{7, 0, 6, 1, 5, 2, 4, 3}, {2, 1, 3, 0, 4, 7, 5, 6}
};

// result outlook
enum Outlook {
  UNKNOWN,
  RED_WON,
  BLUE_WON,
  DRAW,
  RED_WONTWIN,
  BLUE_WONTWIN
};


static std::pair<int, int> operator+(const std::pair<int, int> & l,const std::pair<int, int> & r) {
    return {l.first+r.first,l.second+r.second};
};

typedef std::pair<int, int> Coordinates;


// 8 link descriptors store the properties of a link direction
struct {
	std::pair<int, int> offsets;      // offset of the target peg, e.g. (2, -1) for ENE
	std::vector<std::pair<std::pair<int, int>, int>> blockingLinks;
} typedef LinkDescriptor;

struct Cell {
	int peg = OVERBOARD;
	bool deadPeg[kNumPlayers] = { false, false };
	// bitmap of outgoing links from this peg
	int links = 0;
	// bitmap of blocked outgoing links
	int blockedLinks = 0;

} typedef Cell;

//for sets of pegs that are connected to a borderline (2 sets per player)
typedef bool PegSet[kMaxVirtualBoardSize*kMaxVirtualBoardSize];

struct {
	std::vector<std::vector<Cell>> cell;
	PegSet linkedToStart[kNumPlayers];
	PegSet linkedToEnd[kNumPlayers];
	int boardSize;
	bool ansiColorOutput;
	// for drawcheck only
	std::vector<int> drawPath[kNumPlayers];
	bool canWin[kNumPlayers] = { true, true };
} typedef Board;



// State of an in-play game.
class TwixTState: public State {
public:
	TwixTState(std::shared_ptr<const Game> game);

	TwixTState(const TwixTState&) = default;
	TwixTState& operator=(const TwixTState&) = default;

	int CurrentPlayer() const override { return mCurrentPlayer; };
	std::string ActionToString(int player, Action move) const override;

	std::string ToString() const override;
	bool IsTerminal() const override;
	std::vector<double> Returns() const override;

	std::string InformationState(int player) const override;
	void InformationStateAsNormalizedVector(int player, std::vector<double>* values) const override;

	std::string Observation(int player) const override;
	void ObservationAsNormalizedVector(int player,	std::vector<double> *values) const override;

	std::unique_ptr<State> Clone() const override;

	void UndoAction(int player, Action move) override {};
	std::vector<Action> LegalActions() const override;

protected:
	void DoApplyAction(Action move) override;

private:
	int mCurrentPlayer = 0;         // Player zero goes first
	std::vector<Action> mLegalActions[kNumPlayers];
	Board mBoard;
	int mMoveCounter = 0;
	bool mSwapped = false;
	int mMoveOne;
	int mVirtualBoardSize;
	int mOutlook = Outlook::UNKNOWN;

	double mDiscount = 1.0;
	bool mAnsiColorOutput;
	int mBoardSize;
	bool mDrawCheck;

	void InitializeBoards();
	void UpdateOutlook(Coordinates);

	void SetSurroundingLinks(Coordinates);

	void ExploreLocalGraph(Board *, Coordinates , PegSet *);

	void AddBinaryPlane(std::vector<double>*) const;
	void AddLinkPlane(int, int, std::vector<double>*) const;
	void AddPegPlane(int, std::vector<double>*) const;

};


// Game object.
class TwixTGame: public Game {
public:
	explicit TwixTGame(const GameParameters &params);
	std::unique_ptr<State> NewInitialState() const override {
		return std::unique_ptr<State>(new TwixTState(shared_from_this()));
	}

	int NumDistinctActions() const override { return kMaxBoardSize*kMaxBoardSize; };
	int NumPlayers() const override { return kNumPlayers; };
	double MinUtility() const override { return -1; };
	double UtilitySum() const override { return 0; };
	double MaxUtility() const override { return 1; };

	std::shared_ptr<const Game> Clone() const override {
	    return std::shared_ptr<const Game>(new TwixTGame(*this));
	}

	std::vector<int> ObservationNormalizedVectorShape() const override {
		return {};
	}

    std::vector<int> InformationStateNormalizedVectorShape() const override {
	    static std::vector<int> shape{ kNumPlanes, mBoardSize, (mBoardSize-2) };
	    return shape;
    }

	//int MaxGameLength() const override;
    int MaxGameLength() const { return kMaxBoardSize*kMaxBoardSize - 4 + 1; }

	bool getAnsiColorOutput() const {
		return mAnsiColorOutput;
	}

	int getBoardSize() const {
		return mBoardSize;
	}

private:
	bool mAnsiColorOutput;
	bool mDrawCheck;
	int  mBoardSize;
	int  mMaxGameLength;
	double mDiscount;

};


// helper functions
void initializePegs(Board *);
void initializeDrawPath(Board *b, int);
void initializeLegalActions(Board *b, int, std::vector<Action> *);

std::string coordsToString(Coordinates, int);

void removeLegalAction(std::vector<Action> *, Action);
bool findDrawPath(Board *, Coordinates, int, int, Board);


bool isOnBorderline(Coordinates, int, int);
bool isOnMargin(Coordinates, int);

bool isInPegSet(PegSet *, Coordinates);
void addToPegSet(PegSet *, Coordinates);
bool isLinkInDrawPath(Board *, Coordinates, int, int);
bool isPegInDrawPath(std::vector<int> *, Coordinates, int);
int getAction(Coordinates, int);

void appendBeforeRow(Board *, std::string *, Coordinates);
void appendPegRow(Board *, std::string *, Coordinates);
void appendAfterRow(Board *, std::string *, Coordinates);

void appendLinkChar(Board *, std::string *, Coordinates, enum LinkDirections, std::string);
void appendColorString(std::string *, bool, std::string, std::string);
void appendPegChar(Board *, std::string *, Coordinates);

int getOppDir(int);
int getLinkCode(Coordinates, int, int);
void setBlockers(Board *, Coordinates, Coordinates, LinkDescriptor *, bool);
std::string linkCodeToString(int, int);
void printDrawPath(std::vector<int> *, int, std::string);
Coordinates getNeighbour(Coordinates, Coordinates);


// twixt board:
// * the board has bordSize x bardSize cells
// * the x-axis (cols) points from left to right,
// * the y axis (rows) points from bottom to top
// * moves/actions are labeled by col & row, e.g. C4, F4, D2, ...
// * moves/actions are indexed by boardSize * row + col
// * the virtual board has a margin of 2 rows/cols to avoid overboard checks
// * player1: 0, X, top/bottom;
// * player2: 1, O, left/right;
// * EMPTY = -1, OVERBOARD = -2
//
// example 8 x 8 board: red peg at C5 (col/row: [2,4], virtual [4,6]
//                      red peg at D3 (col/row: [3,6], virtual [5,8]
//                      blue peg at F5 (col/row: [5,4], virtual [7,6]
//
//              A   B   C   D   E   F   G   H
//
//     -2  -2  -2  -2  -2  -2  -2  -2  -2  -2  -2  -2
//
//     -2  -2  -2  -2  -2  -2  -2  -2  -2  -2  -2  -2
//              -------------------------
// 1   -2  -2 |-1  -1  -1  -1  -1  -1  -1 |-1  -2  -2  <-- borderline of player 1 (X)
//            |                           |
// 2   -2  -2 |-1  -1  -1  -1  -1  -1  -1 |-1  -2  -2
//            |                           |
// 3   -2  -2 |-1  -1  -1   0  -1  -1  -1 |-1  -2  -2
//            |                           |
// 4   -2  -2 |-1  -1  -1  -1  -1  -1  -1 |-1  -2  -2
//            |                           |
// 5   -2  -2 |-1  -1   0  -1  -1   1  -1 |-1  -2  -2
//            |                           |
// 6   -2  -2 |-1  -1  -1  -1  -1  -1  -1 |-1  -2  -2
//            |                           |
// 7   -2  -2 |-1  -1  -1  -1  -1  -1  -1 |-1  -2  -2
//            |                           |
// 8   -2  -2 |-1  -1  -1  -1  -1  -1  -1 |-1  -2  -2  <-- borderline of player 1 (X)
//              -------------------------
//     -2  -2  -2  -2  -2  -2  -2  -2  -2  -2  -2  -2
//
//     -2  -2  -2  -2  -2  -2  -2  -2  -2  -2  -2  -2

// links:
// * for each cell we store an int with the bitmap of the 8 possible links
// * the 8 link directions are indexed in clockwise order starting at NNE
// * bit 2**idx is set, if link idx is set
// blocked links:
// * for each peg we also store an int with a bitmap of the blocked links
// * i.e. that cannot be set in the future.

// the blockingLinks vector is to check if an existing link prevents us from setting another link
// we store existing links redundantly on the board, i.e. from both peg's perspective,

// table of 8 link descriptors
static std::vector<LinkDescriptor> kLinkDescriptorTable
{
	// NNE
	{
	   {1,  2},   // offset of target peg (2 up, 1 right)
	   {           // blocking/blocked links
			{{0,  1}, LinkDirections::ENE },
			{{-1, 0}, LinkDirections::ENE },

			{{ 0,  2}, LinkDirections::ESE },
			{{ 0,  1}, LinkDirections::ESE },
			{{-1,  2}, LinkDirections::ESE },
			{{-1,  1}, LinkDirections::ESE },

			{{0,  1}, LinkDirections::SSE },
			{{0,  2}, LinkDirections::SSE },
			{{0,  3}, LinkDirections::SSE }
	   }
	},
	// ENE
	{
	   {2,  1},    // offset of target peg
	   {           // blocking/blocked links
			{{ 0, -1}, LinkDirections::NNE },
			{{ 1,  0}, LinkDirections::NNE },

			{{-1,  1}, LinkDirections::ESE },
			{{ 0,  1}, LinkDirections::ESE },
			{{ 1,  1}, LinkDirections::ESE },

			{{ 0,  1}, LinkDirections::SSE },
			{{ 0,  2}, LinkDirections::SSE },
			{{ 1,  1}, LinkDirections::SSE },
			{{ 1,  2}, LinkDirections::SSE }
	   }
	},
	// ESE
	{
	   { 2, -1},   // offset of target peg
	   {           // blocking/blocked links
			{{ 0, -1}, LinkDirections::NNE },
			{{ 1, -1}, LinkDirections::NNE },
			{{ 0, -2}, LinkDirections::NNE },
			{{ 1, -2}, LinkDirections::NNE },

			{{-1, -1}, LinkDirections::ENE },
			{{ 0, -1}, LinkDirections::ENE },
			{{ 1, -1}, LinkDirections::ENE },

			{{ 0,  1}, LinkDirections::SSE },
			{{ 1,  0}, LinkDirections::SSE }
	   }
	},
	// SSE
	{
	   { 1, -2},   // offset of target peg
	   {           // blocking/blocked links
			{{ 0, -1}, LinkDirections::NNE },
			{{ 0, -2}, LinkDirections::NNE },
			{{ 0, -3}, LinkDirections::NNE },

			{{-1, -1}, LinkDirections::ENE },
			{{ 0, -1}, LinkDirections::ENE },
			{{-1, -2}, LinkDirections::ENE },
			{{ 0, -2}, LinkDirections::ENE },

			{{-1,  0}, LinkDirections::ESE },
			{{ 0, -1}, LinkDirections::ESE }
	   }
	},
	// SSW
	{
	   {-1, -2},    // offset of target peg
	   {           // blocking/blocked links
			{{-1, -1}, LinkDirections::ENE },
			{{-2, -2}, LinkDirections::ENE },

			{{-2,  0}, LinkDirections::ESE },
			{{-1,  0}, LinkDirections::ESE },
			{{-2, -1}, LinkDirections::ESE },
			{{-1, -1}, LinkDirections::ESE },

			{{-1,  1}, LinkDirections::SSE },
			{{-1,  0}, LinkDirections::SSE },
			{{-1, -1}, LinkDirections::SSE }
	   }
	},
	// WSW
	{
	   {-2, -1},   // offset of target peg
	   {           // blocking/blocked links
			{{-2, -2}, LinkDirections::NNE },
			{{-1, -1}, LinkDirections::NNE },

			{{-3,  0}, LinkDirections::ESE },
			{{-2,  0}, LinkDirections::ESE },
			{{-1,  0}, LinkDirections::ESE },

			{{-2,  1}, LinkDirections::SSE },
			{{-1,  1}, LinkDirections::SSE },
			{{-2,  0}, LinkDirections::SSE },
			{{-1,  0}, LinkDirections::SSE }
	   }
	},
	// WNW
	{
	   {-2, 1},   // offset of target peg
	   {           // blocking/blocked links
			{{-2,  0}, LinkDirections::NNE },
			{{-1,  0}, LinkDirections::NNE },
			{{-2, -1}, LinkDirections::NNE },
			{{-1, -1}, LinkDirections::NNE },

			{{-3,  0}, LinkDirections::ENE },
			{{-2,  0}, LinkDirections::ENE },
			{{-1,  0}, LinkDirections::ENE },

			{{-2,  2}, LinkDirections::SSE },
			{{-1,  1}, LinkDirections::SSE }
	   }
	},
	// NNW
	{
	   {-1, 2},   // offset of target peg
	   {           // blocking/blocked links
			{{-1,  1}, LinkDirections::NNE },
			{{-1,  0}, LinkDirections::NNE },
			{{-1, -1}, LinkDirections::NNE },

			{{-2,  1}, LinkDirections::ENE },
			{{-1,  1}, LinkDirections::ENE },
			{{-2,  0}, LinkDirections::ENE },
			{{-1,  0}, LinkDirections::ENE },

			{{-2,  2}, LinkDirections::ESE },
			{{-1,  1}, LinkDirections::ESE }
	   }
	}

};



}  // namespace twixt
}  // namespace open_spiel



#endif  // THIRD_PARTY_OPEN_SPIEL_GAMES_TWIXT_H_
