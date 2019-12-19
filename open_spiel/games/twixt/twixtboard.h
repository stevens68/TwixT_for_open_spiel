
#ifndef THIRD_PARTY_OPEN_SPIEL_GAMES_TWIXTBOARD_H_
#define THIRD_PARTY_OPEN_SPIEL_GAMES_TWIXTBOARD_H_

#include "open_spiel/spiel.h"
#include "open_spiel/games/twixt/twixtcell.h"

#include <vector>
#include <string>
#include <map>

namespace open_spiel {
namespace twixt {

const int kMinBoardSize =5 ;
const int kMaxBoardSize = 24;
const int kDefaultBoardSize = 8;

const int kDefaultAnsiColorOutput=true;

const double kMinDiscount=0.0;
const double kMaxDiscount=1.0;
const double kDefaultDiscount=kMaxDiscount;

// 8 link descriptors store the properties of a link direction
struct {
	Tuple offsets; // offset of the target peg, e.g. (2, -1) for ENE
	std::vector<std::pair<Tuple, int>> blockingLinks;
} typedef LinkDescriptor;

const int kNumPlanes=11;  // 2 * (1 for pegs + 4 for links) + 1 for player to move

enum Result {
  OPEN,
  RED_WON,
  BLUE_WON,
  DRAW,
  RESULT_COUNT
};

enum Color {
	COLOR_RED,
	COLOR_BLUE,
	EMPTY,
	OVERBOARD,
	COLOR_COUNT
};

static std::map<Link, std::set<Link>> blockerMap;


inline std::set<Link>* getBlockers(Link link)  {
	return &blockerMap[link];
};

inline void pushBlocker(Link link, Link blockedLink ) {
	blockerMap[link].insert(blockedLink);
};

inline void deleteBlocker(Link link, Link blockedLink ) {
	blockerMap[link].erase(blockedLink);
};

inline void clearBlocker() {
	blockerMap.clear();
};


class Board {

	private:
		int mMoveCounter = 0;
		bool mSwapped = false;
		int mMoveOne;
		int mResult = Result::OPEN;
		std::vector<std::vector<Cell>> mCell;
		int mSize;
		bool mAnsiColorOutput;
		std::vector<Action> mLegalActions[PLAYER_COUNT];
		int mLegalActionIndex[PLAYER_COUNT][kMaxBoardSize*kMaxBoardSize];
		std::vector<double> mTensor;
		std::set<Link> mBlockedLinks;

		void setSize(int size) { mSize = size; };
		int getSize() const { return mSize; };

		bool getAnsiColorOutput() const { return mAnsiColorOutput; };
		void setAnsiColorOutput (bool ansiColorOutput) { mAnsiColorOutput = ansiColorOutput; };

		void setResult(int result) { mResult = result; }

		bool getSwapped() const { return mSwapped; };
		void setSwapped(bool swapped) {	mSwapped = swapped; };

		Action getMoveOne() const { return mMoveOne; };
		void setMoveOne(Action action) { mMoveOne = action; };

		void incMoveCounter() {	mMoveCounter++; };

		const Cell* getConstCell(Tuple c) const { return  &(mCell[c.first][c.second]); };
		Cell* getCell(Tuple c) { return  &(mCell[c.first][c.second]); };

		bool hasLegalActions(int player) const { return mLegalActions[player].size() > 0; };
		void removeLegalAction(int player, Action action) {
			// for the performance optimized version
			// the basic test fails because actions are not sorted
			// but it works for example.cc and mcts_example.cc
			/*
			int pos = mLegalActionIndex[player][action];
			if (pos >= 0) {
				std::vector<Action> *la = &mLegalActions[player];
				int last = la->back();
				(*la)[pos] = last;
				mLegalActionIndex[player][last] = pos;
				la->pop_back();
			}
			*/
			std::vector<Action> *la = &mLegalActions[player];
			std::vector<Action>::iterator it;
			it = find(la->begin(), la->end(), action);
			if (it != la->end()) la->erase(it);
		};

		void updateResult(int, Tuple);
		void undoFirstMove(Tuple c);

		void initializeCells(bool);
		void initializeCandidates(Tuple, Cell *, bool);
		void initializeBlockerMap(Tuple, int, LinkDescriptor *);

		void initializeLegalActions();

		void addBinaryPlane(int, std::vector<double> *) const;
		void addLinkPlane(int, int, std::vector<double> *) const;
		void addPegPlane(int, std::vector<double> *) const;

		void setPegAndLinks(int, Tuple);
		void exploreLocalGraph(int, Cell * , enum Border);

		void appendLinkChar(std::string *, Tuple, enum Compass, std::string) const;
		void appendColorString(std::string *, std::string, std::string) const;
		void appendPegChar(std::string *, Tuple ) const;

		void appendBeforeRow(std::string *, Tuple) const;
		void appendPegRow(std::string *, Tuple) const;
		void appendAfterRow(std::string *, Tuple) const;

		bool coordsOnBorder(int, Tuple) const;
		bool coordsOffBoard(Tuple) const;

		int stringToAction(std::string s) const {
			int x = int(s.at(0)) - int('A');
			int y = getSize() - (int(s.at(1)) - int('0'));
			return y * getSize() + x;
		};

	public:
		~Board() {};
		Board() {};
		Board(int, bool);

		std::string actionToString(Action) const;
		std::string toString() const;
		int getResult() const {	return mResult; };
		int getMoveCounter() const { return mMoveCounter; };
		void createTensor(int, std::vector<double> *) const;
		std::vector<Action> getLegalActions(int player) const { return mLegalActions[player]; };
		void applyAction(int, Action);
};


// twixt board:
// * the board has bordSize x bardSize cells
// * the x-axis (cols) points from left to right,
// * the y axis (rows) points from bottom to top
// * moves are labeled by col / row, e.g. C3, F4, D2, ...
//   (top row=1, left col=A)
// * coordinates => move: y * boardSize + x
// * player1: 0, X, top/bottom, colored red
// * player2: 1, O, left/right, colored blue
// * empty cell = 2 (EMPTY)
// * corner cell = 3 (OVERBORD)
//
// example 8 x 8 board: red peg at C5 x/y: [0,2]
//                      red peg at D3 x/y: [1,4]
//                     blue peg at F5 x/y: [3,2]
//
//     A   B   C   D   E   F   G   H
//    ------------------------------
// 1 | 3   2   2   2   2   2   2   3 |
//   |                               |
// 2 | 2   2   2   2   2   2   2   2 |
//   |                               |
// 3 | 2   2   2   0   2   2   2   2 |
//   |                               |
// 4 | 2   2   2   2   2   2   2   2 |
//   |                               |
// 5 | 2   2   0   2   2   1   2   2 |
//   |                               |
// 6 | 2   2   2   2   2   2   2   2 |
//   |                               |
// 7 | 2   2   2   2   2   2   2   2 |
//   |                               |
// 8 | 3   2   2   2   2   2   2   3 |
//     ------------------------------

}  // namespace twixt
}  // namespace open_spiel

#endif  // THIRD_PARTY_OPEN_SPIEL_GAMES_TWIXTBOARD_H_
