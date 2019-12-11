
#ifndef THIRD_PARTY_OPEN_SPIEL_GAMES_TWIXTCELL_H_
#define THIRD_PARTY_OPEN_SPIEL_GAMES_TWIXTCELL_H_


namespace open_spiel {
namespace twixt {



class Cell {
	private:
		int mPeg;
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


}  // namespace twixt
}  // namespace open_spiel



#endif  // THIRD_PARTY_OPEN_SPIEL_GAMES_TWIXTCELL_H_
