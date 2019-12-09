
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
//#include "open_spiel/games/twixtboard.h"

// https://en.wikipedia.org/wiki/TwixT

namespace open_spiel {
namespace twixt {

const int kNumPlayers = 2;
const int kNumSides = 2;
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

// ANSI colors
const std::string kAnsiRed = "\e[91m";
const std::string kAnsiBlue = "\e[94m";
const std::string kAnsiDefault = "\e[0m";

// colors
const int RED = 0;  // player, peg color, board index
const int BLUE = 1; // player, peg color, board index
const int EMPTY = -2; // peg color
const int OVERBOARD = -3; // peg color


// eight directions of links from 0 to 7
enum LinkDirections {
  NNE = 0,  // North-North-East, 2 up, 1 right
  ENE = 1,  // East-North-East,  1 up, 2 right
  ESE = 2,  // East-South-East,  1 down, 2 right
  SSE = 3,  // South-South-East, 2 down, 1 right
  SSW = 4,  // South-South-West, 2 down, 1 left
  WSW = 5,  // West-South-West,  1 down, 2 left
  WNW = 6,  // West-North-West,  1 up, 2 left
  NNW = 7   // North-North-West, 2 up, 1 left
};

enum Border {
	START = 0,
	END = 1
};

const int LINKORDER[kNumPlayers][kNumLinkDirections] = {
	{7, 0, 6, 1, 5, 2, 4, 3},
	{2, 1, 3, 0, 4, 7, 5, 6}
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
	Coordinates offsets;      // offset of the target peg, e.g. (2, -1) for ENE
	std::vector<std::pair<Coordinates, int>> blockingLinks;
} typedef LinkDescriptor;


//for sets of pegs that are connected to a borderline (2 sets per player)
typedef bool PegSet[kMaxVirtualBoardSize*kMaxVirtualBoardSize];

class Cell {
	private:
		int mPeg = OVERBOARD;
		// bitmap of outgoing links from this peg
		int mLinks = 0;
		// bitmap of blocked outgoing links
		int mBlockedLinks = 0;

	public:
		int getPeg() const;
		void setPeg(int);
		void clearLinks();
		void clearBlockedLinks();
		void setLink(int);
		void setBlocked(int);
		bool hasLink(int) const;
		bool hasLinks() const;
		bool isBlocked(int) const;
		bool isDead(int) const;
		void setDead(int);

};


class Board {

	private:
		int mMoveCounter = 0;
		bool mSwapped = false;
		int mMoveOne;
		int mOutlook = Outlook::UNKNOWN;
		std::vector<std::vector<Cell>> mCell;
		PegSet mLinkedToBorder[kNumPlayers][kNumSides];
		int mSize;
		int mVirtualSize;
		bool mAnsiColorOutput;
		std::vector<Action> mLegalActions[kNumPlayers];
		std::vector<double> mNormalizedVector;


		void setSize(int);
		int getSize() const;

		void setVirtualSize(int);
		int getVirtualSize() const;

		bool getAnsiColorOutput() const;
		void setAnsiColorOutput(bool);

		void setOutlook(int);

		bool getSwapped() const;
		void setSwapped(bool);

		Action getMoveOne() const;
		void setMoveOne(Action);

		void incMoveCounter();


		void setBlockers(Coordinates, LinkDescriptor *);

		bool isInPegSet(int, enum Border, Coordinates) const;
		void addToPegSet(int, enum Border, Coordinates);

		const Cell* getConstCell(Coordinates) const;
		Cell* getCell(Coordinates);

		void updateOutlook(int, Coordinates);

		void initialize();
		void initializePegs();
		void initializeLegalActions(int);

		bool hasLegalActions(int) const;
		void removeLegalAction(int, Action);

		void addBinaryPlane(int, std::vector<double> *) const;
		void addLinkPlane(int, int, std::vector<double> *) const;
		void addPegPlane(int, std::vector<double> *) const;

		void setSurroundingLinks(int, Coordinates);
		void exploreLocalGraph(int, Coordinates , enum Border);

		void appendLinkChar(std::string *, Coordinates, enum LinkDirections, std::string) const;
		void appendColorString(std::string *, std::string, std::string) const;
		void appendPegChar(std::string *, Coordinates ) const;

		void appendBeforeRow(std::string *, Coordinates) const;
		void appendPegRow(std::string *, Coordinates) const;
		void appendAfterRow(std::string *, Coordinates) const;


	public:
		~Board();
		Board();
		Board(int, bool);

		std::string actionToString(int, Action) const;
		std::string toString() const;
		int getOutlook() const;
		int getMoveCounter() const;
		void createNormalizedVector(int, std::vector<double> *) const;
		std::vector<Action> getLegalActions(int) const;
		void applyAction(int, Action);

};


class TwixTState: public State {
	public:
		~TwixTState();
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
		int mCurrentPlayer = RED;         // Player zero goes first
		Board mBoard;
		double mDiscount = kDefaultDiscount;

};


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

		double getDiscount() const {
			return mDiscount;
		}

	private:
		bool mAnsiColorOutput;
		int  mBoardSize;
		double mDiscount;

};

// helper functions

std::string coordsToString(Coordinates, int);
bool isOnBorderline(Coordinates, int, int);
bool isOnMargin(Coordinates, int);
int getAction(Coordinates, int);

int getOppDir(int);
int getLinkCode(Coordinates, int, int);
std::string linkCodeToString(int, int);
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
