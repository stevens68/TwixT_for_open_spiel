
#ifndef THIRD_PARTY_OPEN_SPIEL_GAMES_TWIXTCELL_H_
#define THIRD_PARTY_OPEN_SPIEL_GAMES_TWIXTCELL_H_


namespace open_spiel {
namespace twixt {


// player
enum Player {
	PLAYER_RED,
	PLAYER_BLUE,
	PLAYER_COUNT
};

class Cell {
	private:
		int mPeg;
		// bitmap of outgoing links from this peg
		int mLinks = 0;
		// bitmap of blocked outgoing links
		int mBlockedLinks[PLAYER_COUNT] = { 0, 0 };

	public:
		int getPeg() const { return mPeg; };
		void setPeg(int color) { mPeg = color; };
		void clearLinks() { mLinks = 0; };
		void setLink(int dir) { mLinks |= 1UL << dir; };
		bool hasLink(int dir) const { return mLinks & 1UL << dir; };
		bool hasLinks() const { return mLinks > 0; };

		void clearBlockedLinks() {
			mBlockedLinks[PLAYER_RED] = 0;
			mBlockedLinks[PLAYER_BLUE] = 0;
		};
		void setBlocked(int player, int dir) { mBlockedLinks[player] |= 1UL << dir;  };
		void setBlocked(int dir) { setBlocked(PLAYER_RED, dir); setBlocked(PLAYER_BLUE, dir); };
		bool isBlocked(int player, int dir) const { return mBlockedLinks[player] & 1UL << dir; };
};


}  // namespace twixt
}  // namespace open_spiel



#endif  // THIRD_PARTY_OPEN_SPIEL_GAMES_TWIXTCELL_H_
