
#ifndef THIRD_PARTY_OPEN_SPIEL_GAMES_TWIXTBOARD_H_
#define THIRD_PARTY_OPEN_SPIEL_GAMES_TWIXTBOARD_H_

#include "open_spiel/spiel.h"
#include "open_spiel/games/twixt/twixtcell.h"

#include <vector>
#include <string>

namespace open_spiel {
namespace twixt {

const int kMinBoardSize =5 ;
const int kMaxBoardSize = 24;
const int kDefaultBoardSize = 8;

const int kMargin = 2;
const int kMaxVirtualBoardSize = kMaxBoardSize + 2 * kMargin;

const int kDefaultAnsiColorOutput=true;

const double kMinDiscount=0.0;
const double kMaxDiscount=1.0;
const double kDefaultDiscount=kMaxDiscount;


typedef std::pair<int, int> Coordinates;

// 8 link descriptors store the properties of a link direction
struct {
	Coordinates offsets;      // offset of the target peg, e.g. (2, -1) for ENE
	std::vector<std::pair<Coordinates, int>> blockingLinks;
} typedef LinkDescriptor;


const int kNumPlanes=11;  // 2 * (1 for pegs + 4 for links) + 1 for player to move


enum Result {
  OPEN,
  RED_WON,
  BLUE_WON,
  DRAW,
  RESULT_COUNT
};



// colors
enum Color {
	COLOR_RED,
	COLOR_BLUE,
	EMPTY,
	OVERBOARD,
	COLOR_COUNT
};

enum Border {
	START,
	END,
	BORDER_COUNT
};

// eight directions of links from 0 to 7
enum Compass {
  NNE,  // North-North-East, 2 up,   1 right
  ENE,  // East-North-East,  1 up,   2 right
  ESE,  // East-South-East,  1 down, 2 right
  SSE,  // South-South-East, 2 down, 1 right
  SSW,  // South-South-West, 2 down, 1 left
  WSW,  // West-South-West,  1 down, 2 left
  WNW,  // West-North-West,  1 up,   2 left
  NNW,  // North-North-West, 2 up,   1 left
  COMPASS_COUNT
};

typedef bool PegSet[kMaxVirtualBoardSize*kMaxVirtualBoardSize];

class Board {

	private:
		int mMoveCounter = 0;
		bool mSwapped = false;
		int mMoveOne;
		int mResult = Result::OPEN;
		std::vector<std::vector<Cell>> mCell;
		PegSet mLinkedToBorder[PLAYER_COUNT][BORDER_COUNT];
		int mSize;
		int mVirtualSize;
		bool mAnsiColorOutput;
		std::vector<Action> mLegalActions[PLAYER_COUNT];
		int mLegalActionIndex[PLAYER_COUNT][kMaxBoardSize*kMaxBoardSize];
		std::vector<double> mNormalizedVector;


		void setSize(int size) {	mSize = size; };
		int getSize() const { return mSize; };

		void setVirtualSize(int virtualSize) { mVirtualSize = virtualSize; };
		int getVirtualSize() const { return mVirtualSize; };

		bool getAnsiColorOutput() const { return mAnsiColorOutput; };
		void setAnsiColorOutput (bool ansiColorOutput) { mAnsiColorOutput = ansiColorOutput; };

		void setResult(int result) { mResult = result; }

		bool getSwapped() const { return mSwapped; };
		void setSwapped(bool swapped) {	mSwapped = swapped; };

		Action getMoveOne() const { return mMoveOne; };
		void setMoveOne(Action action) { mMoveOne = action; };

		void incMoveCounter() {	mMoveCounter++; };


		bool isInPegSet(int player, enum Border border, Coordinates c) const {
			return mLinkedToBorder[player][border][c.second * getVirtualSize() + c.first];
		};
		void addToPegSet(int player, enum Border border, Coordinates c) {
			mLinkedToBorder[player][border][c.second * getVirtualSize() + c.first] = true;
		};

		const Cell* getConstCell(Coordinates c) const { return  &(mCell[c.first][c.second]); };
		Cell* getCell(Coordinates c) { return  &(mCell[c.first][c.second]); };

		bool hasLegalActions(int player) const { return mLegalActions[player].size() > 0; };
		/*
		void removeLegalAction(int player, Action action) {
			std::vector<Action> *la = &mLegalActions[player];
			std::vector<Action>::iterator it = find(la->begin(), la->end(), action);
		    if (it != la->end()) la->erase(it);
		};
		*/
		void removeLegalAction(int player, Action action) {
			int pos = mLegalActionIndex[player][action];
			if (pos >= 0) {
				std::vector<Action> *la = &mLegalActions[player];
				int last = la->back();
				(*la)[pos] = last;
				mLegalActionIndex[player][last] = pos;
				la->pop_back();
			}
		};


		void setBlockers(Coordinates, LinkDescriptor *);

		void updateResult(int, Coordinates);

		void initialize();
		void initializePegs();
		void initializeLegalActions();


		void addBinaryPlane(int, std::vector<double> *) const;
		void addLinkPlane(int, int, std::vector<double> *) const;
		void addPegPlane(int, std::vector<double> *) const;

		void setSurroundingLinks(int, Coordinates);
		void exploreLocalGraph(int, Coordinates , enum Border);

		void appendLinkChar(std::string *, Coordinates, enum Compass, std::string) const;
		void appendColorString(std::string *, std::string, std::string) const;
		void appendPegChar(std::string *, Coordinates ) const;

		void appendBeforeRow(std::string *, Coordinates) const;
		void appendPegRow(std::string *, Coordinates) const;
		void appendAfterRow(std::string *, Coordinates) const;

	public:
		~Board() {};
		Board() {};
		Board(int, bool);

		std::string actionToString(int, Action) const;
		std::string toString() const;
		int getResult() const {	return mResult; };
		int getMoveCounter() const { return mMoveCounter; };
		void createNormalizedVector(int, std::vector<double> *) const;
		std::vector<Action> getLegalActions(int player) const { return mLegalActions[player]; };
		void applyAction(int, Action);
};


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


}  // namespace twixt
}  // namespace open_spiel



#endif  // THIRD_PARTY_OPEN_SPIEL_GAMES_TWIXTBOARD_H_
