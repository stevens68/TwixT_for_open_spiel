
#ifndef THIRD_PARTY_OPEN_SPIEL_GAMES_TWIXTCELL_H_
#define THIRD_PARTY_OPEN_SPIEL_GAMES_TWIXTCELL_H_


typedef std::pair<int, int> Tuple;
typedef std::pair<Tuple, int> Link;

namespace open_spiel {
namespace twixt {

enum Border {
	START,
	END,
	BORDER_COUNT
};

enum Player {
	PLAYER_RED,
	PLAYER_BLUE,
	PLAYER_COUNT
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

class Cell {
	private:
		int mColor;
		// bitmap of outgoing links from this cell
		int mLinks = 0;
		// bitmap of neighbors of for player
		int mCandidates[PLAYER_COUNT] = { 0, 0 };
		// array of neighbor tuples
		Tuple mNeighbors[COMPASS_COUNT];
		// indicator if cell is linked to START|END line of player
		bool mLinkedToBorder[PLAYER_COUNT][BORDER_COUNT] = { {false, false}, {false, false} };

	public:
		int getColor() const { return mColor; };
		void setColor(int color) { mColor = color; };

		void clearLinks() { mLinks = 0; };
		void setLink(int dir) { mLinks |= (1UL << dir); };
		bool isLinked(int cand) const { return mLinks & cand; };
		int getLinks() const { return mLinks; };


		bool hasLink(int dir) const { return mLinks & (1UL << dir); };
		bool hasLinks() const { return mLinks > 0; };

		int getCandidates(int player) { return mCandidates[player]; }
		bool isCandidate(int player, int cand) const { return mCandidates[player] & cand; }
		void setCandidate(int player, int dir) { mCandidates[player] |= (1UL << dir); };
		void deleteCandidate(int player, int cand) { mCandidates[player] &= ~(cand); };
		void deleteCandidate(int dir) {
			mCandidates[PLAYER_RED] &= ~(1UL << dir);
			mCandidates[PLAYER_BLUE] &= ~(1UL << dir);
		};

		Tuple getNeighbor(int dir) const { return mNeighbors[dir]; };
		void setNeighbor(int dir, Tuple c) { mNeighbors[dir]=c; };

		void setLinkedToBorder(int player, int border) { mLinkedToBorder[player][border] = true; };
	    bool isLinkedToBorder(int player, int border) const { return mLinkedToBorder[player][border]; };

};


}  // namespace twixt
}  // namespace open_spiel



#endif  // THIRD_PARTY_OPEN_SPIEL_GAMES_TWIXTCELL_H_
