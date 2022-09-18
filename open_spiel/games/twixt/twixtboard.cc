#include "open_spiel/games/twixt/twixtboard.h"
#include "open_spiel/games/twixt/twixtcell.h"

using namespace std;

namespace open_spiel {
namespace twixt {

// ANSI colors
const string kAnsiRed = "\e[91m";
const string kAnsiBlue = "\e[94m";
const string kAnsiDefault = "\e[0m";

static pair<int, int> operator+(const pair<int, int> & l,const pair<int, int> & r) {
    return { l.first + r.first, l.second + r.second };
};

// helper functions
inline int oppDir(int dir) {
	return (dir + kMaxCompass / 2) % kMaxCompass;
}

inline int oppCand(int cand) {
	return cand < 16 ? cand<<=4 : cand>>=4;
}

inline std::string coordsToString(Tuple c) {
	return "[" + std::to_string(c.first) + "," + std::to_string(c.second) + "]";
}

// table of 8 link descriptors
static vector<LinkDescriptor> kLinkDescriptorTable
{
	// NNE
	{
	   {1,  2},   // offset of target peg (2 up, 1 right)
	   {          // blocking/blocked links
			{{ 0,  1}, kENE },
			{{-1,  0}, kENE },

			{{ 0,  2}, kESE },
			{{ 0,  1}, kESE },
			{{-1,  2}, kESE },
			{{-1,  1}, kESE },

			{{ 0,  1}, kSSE },
			{{ 0,  2}, kSSE },
			{{ 0,  3}, kSSE }
	   }
	},
	// ENE
	{
	   {2,  1},
	   {
			{{ 0, -1}, kNNE },
			{{ 1,  0}, kNNE },

			{{-1,  1}, kESE },
			{{ 0,  1}, kESE },
			{{ 1,  1}, kESE },

			{{ 0,  1}, kSSE },
			{{ 0,  2}, kSSE },
			{{ 1,  1}, kSSE },
			{{ 1,  2}, kSSE }
	   }
	},
	// ESE
	{
	   { 2, -1},
	   {
			{{ 0, -1}, kNNE },
			{{ 1, -1}, kNNE },
			{{ 0, -2}, kNNE },
			{{ 1, -2}, kNNE },

			{{-1, -1}, kENE },
			{{ 0, -1}, kENE },
			{{ 1, -1}, kENE },

			{{ 0,  1}, kSSE },
			{{ 1,  0}, kSSE }
	   }
	},
	// SSE
	{
	   { 1, -2},
	   {
			{{ 0, -1}, kNNE },
			{{ 0, -2}, kNNE },
			{{ 0, -3}, kNNE },

			{{-1, -1}, kENE },
			{{ 0, -1}, kENE },
			{{-1, -2}, kENE },
			{{ 0, -2}, kENE },

			{{-1,  0}, kESE },
			{{ 0, -1}, kESE }
	   }
	},
	// SSW
	{
	   {-1, -2},
	   {
			{{-1, -1}, kENE },
			{{-2, -2}, kENE },

			{{-2,  0}, kESE },
			{{-1,  0}, kESE },
			{{-2, -1}, kESE },
			{{-1, -1}, kESE },

			{{-1,  1}, kSSE },
			{{-1,  0}, kSSE },
			{{-1, -1}, kSSE }
	   }
	},
	// WSW
	{
	   {-2, -1},
	   {
			{{-2, -2}, kNNE },
			{{-1, -1}, kNNE },

			{{-3,  0}, kESE },
			{{-2,  0}, kESE },
			{{-1,  0}, kESE },

			{{-2,  1}, kSSE },
			{{-1,  1}, kSSE },
			{{-2,  0}, kSSE },
			{{-1,  0}, kSSE }
	   }
	},
	// WNW
	{
	   {-2, 1},
	   {
			{{-2,  0}, kNNE },
			{{-1,  0}, kNNE },
			{{-2, -1}, kNNE },
			{{-1, -1}, kNNE },

			{{-3,  0}, kENE },
			{{-2,  0}, kENE },
			{{-1,  0}, kENE },

			{{-2,  2}, kSSE },
			{{-1,  1}, kSSE }
	   }
	},
	// NNW
	{
	   {-1, 2},
	   {
			{{-1,  1}, kNNE },
			{{-1,  0}, kNNE },
			{{-1, -1}, kNNE },

			{{-2,  1}, kENE },
			{{-1,  1}, kENE },
			{{-2,  0}, kENE },
			{{-1,  0}, kENE },

			{{-2,  2}, kESE },
			{{-1,  1}, kESE }
	   }
	}

};



Board::Board(int size, bool ansiColorOutput) {
	setSize(size);
	setAnsiColorOutput(ansiColorOutput);

	initializeCells(true);
	initializeLegalActions();
}

void Board::initializeBlockerMap(Tuple c, int dir, LinkDescriptor *ld) {

	Link link = { c, dir };
	for (auto &&entry : ld->blockingLinks) {
		Tuple fromCoords = c + entry.first;
		if (! coordsOffBoard(fromCoords)) {
			LinkDescriptor *oppLd = &(kLinkDescriptorTable[entry.second]);
			Tuple toCoords = c + entry.first + oppLd->offsets;
			if (! coordsOffBoard(toCoords)) {
				pushBlocker(link, { fromCoords, entry.second });
				pushBlocker(link, { toCoords, oppDir(entry.second) });
			}
		}
	}
}

void Board::updateResult(int player, Tuple c) {

	// check for WIN
	bool connectedToStart = getCell(c)->isLinkedToBorder(player, kStart);
	bool connectedToEnd = getCell(c)->isLinkedToBorder(player, kEnd);
	if (connectedToStart && connectedToEnd) {
		// peg is linked to both boarder lines
		setResult(player == kRedPlayer ? kRedWin : kBlueWin);
		return;
	}

	// check if we are early in the game...
	if (getMoveCounter() < getSize() - 1) {
		// e.g. less than 5 moves played on a 6x6 board
		// => no win or draw possible, no need to update
		return;
	}

	//check if opponent (player to turn next) has any legal moves left
	if (! hasLegalActions(1 - player)) {
		setResult(kDraw);
		return;
	}
}

void Board::initializeCells(bool initBlockerMap) {

	mCell.resize(getSize(), vector<Cell>(getSize()));
	clearBlocker();

	// initialize board with color (empty or overboard)
	for (int x = 0; x < getSize(); x++) {
		for (int y = 0; y < getSize(); y++) {

			Tuple c = {x, y};
			Cell *pCell = getCell(c);

			// set color to EMPTY or OVERBOARD
			if (coordsOffBoard(c)) {
				pCell->setColor(kOffBoard);
			} else { // regular board
				pCell->setColor(kEmpty);
				if (x == 0) {
					pCell->setLinkedToBorder(kBluePlayer, kStart);
				} else if (x == getSize()-1) {
					pCell->setLinkedToBorder(kBluePlayer, kEnd);
				} else if (y == 0) {
					pCell->setLinkedToBorder(kRedPlayer, kStart);
				} else if (y == getSize()-1) {
					pCell->setLinkedToBorder(kRedPlayer, kEnd);
				}

				initializeCandidates(c, pCell, initBlockerMap);
			}
		}
	}


}

void Board::initializeCandidates(Tuple c, Cell *pCell, bool initBlockerMap) {

	for (int dir = 0; dir < kMaxCompass; dir++) {
		LinkDescriptor *ld = &(kLinkDescriptorTable[dir]);
		Tuple tc = c + ld->offsets;
		if (! coordsOffBoard(tc)) {
			if (initBlockerMap) {
				initializeBlockerMap(c, dir, ld);
			}
			pCell->setNeighbor(dir, tc);
			Cell *pTargetCell = getCell(tc);
			if (! (coordsOnBorder(kRedPlayer, c) && coordsOnBorder(kBluePlayer, tc)) &&
				! (coordsOnBorder(kBluePlayer, c) && coordsOnBorder(kRedPlayer, tc))) {
					pCell->setCandidate(kRedPlayer, dir);
					pCell->setCandidate(kBluePlayer, dir);
			}
		}
	}
}

void Board::initializeLegalActions() {

	for (int player = kRedPlayer; player < kMaxPlayer; player++) {
		vector<Action> *la = &mLegalActions[player];

		la->clear();
		la->reserve(getSize() * getSize());

		for (int y = 0; y < getSize(); y++) {
			for (int x = 0; x < getSize(); x++) {
				int action = y * getSize() + x;
				if ( ! coordsOnBorder(1 - player, {x, y})
						&& ! coordsOffBoard({x, y})) {
					mLegalActionIndex[player][action] = la->size();
					la->push_back(action);
				} else {
					mLegalActionIndex[player][action] = -1;
				}
			}
		}
	}
}

string Board::toString() const {

	string s = "";

	// head line
	s.append("     ");
	for (int y = 0; y < getSize(); y++) {
		string letter = "";
		letter += char(int('A') + y);
		letter += "  ";
		appendColorString(&s, kAnsiRed, letter);
	}
	s.append("\n");

	for (int y = getSize() -1; y >= 0; y--) {
		// print "before" row
		s.append("    ");
		for (int x = 0; x < getSize(); x++) {
			appendBeforeRow(&s, {x, y});
		}
		s.append("\n");

		// print "peg" row
		getSize() - y < 10 ? s.append("  ") : s.append(" ");
		appendColorString(&s, kAnsiBlue, to_string(getSize() - y) + " ");
		for (int x = 0; x < getSize(); x++) {
			appendPegRow(&s, {x, y});
		}
		s.append("\n");

		// print "after" row
		s.append("    ");
		for (int x = 0; x < getSize(); x++) {
			appendAfterRow(&s, {x, y});
		}
		s.append("\n");
	}
	s.append("\n");

	if (mSwapped)
		s.append("[swapped]");

	switch (mResult) {
	case kOpen:
		break;
	case kRedWin:
		s.append("[X has won]");
		break;
	case kBlueWin:
		s.append("[O has won]");
		break;
	case kDraw:
		s.append("[draw]");
	default:
		break;
	}

	return s;
}

void Board::appendLinkChar(string *s, Tuple c, enum Compass dir, string linkChar) const {
	if (! coordsOffBoard(c) && getConstCell(c)->hasLink(dir)) {
		if (getConstCell(c)->getColor() == kRedColor) {
			appendColorString(s, kAnsiRed, linkChar);
		} else if (getConstCell(c)->getColor() == kBlueColor) {
			appendColorString(s, kAnsiBlue, linkChar);
		} else {
			s->append(linkChar);
		}
	}
}

void Board::appendColorString(string *s, string colorString, string appString) const {

	s->append(getAnsiColorOutput() ? colorString : ""); // make it colored
	s->append(appString);
	s->append(getAnsiColorOutput() ? kAnsiDefault : ""); // make it default
}

void Board::appendPegChar(string *s, Tuple c) const {
	if (getConstCell(c)->getColor() == kRedColor) {
		// X
		appendColorString(s, kAnsiRed, "X");
	} else if (getConstCell(c)->getColor() == kBlueColor) {
		// O
		appendColorString(s, kAnsiBlue, "O");
	} else if (coordsOffBoard(c)) {
		// corner
		s->append(" ");
	} else if (c.first == 0 || c.first == getSize() - 1) {
		// empty . (blue border line)
		appendColorString(s, kAnsiBlue, ".");
	} else if (c.second == 0 || c.second == getSize() - 1) {
		// empty . (red border line)
		appendColorString(s, kAnsiRed, ".");
	} else {
		// empty (non border line)
		s->append(".");
	}
}

void Board::appendBeforeRow(string *s, Tuple c) const {

	// -1, +1
	int len = s->length();
	appendLinkChar(s, c + (Tuple) {-1, 0}, kENE, "/");
	appendLinkChar(s, c + (Tuple) {-1,-1}, kNNE, "/");
	appendLinkChar(s, c + (Tuple) { 0, 0}, kWNW, "_");
	if (len == s->length()) s->append(" ");

	//  0, +1
	len = s->length();
	appendLinkChar(s, c, kNNE, "|");
	if (len == s->length())	appendLinkChar(s, c, kNNW, "|");
	if (len == s->length()) s->append(" ");

	// +1, +1
	len = s->length();
	appendLinkChar(s, c + (Tuple) {+1, 0}, kWNW, "\\");
	appendLinkChar(s, c + (Tuple) {+1,-1}, kNNW, "\\");
	appendLinkChar(s, c + (Tuple) { 0, 0}, kENE, "_");
	if (len == s->length())	s->append(" ");

}

void Board::appendPegRow(string *s, Tuple c) const {

	// -1, 0
	int len = s->length();
	appendLinkChar(s, c + (Tuple) {-1,-1}, kNNE, "|");
	appendLinkChar(s, c + (Tuple) { 0, 0}, kWSW, "_");
	if (len == s->length()) s->append(" ");

	//  0,  0
	appendPegChar(s, c);

	// +1, 0
	len = s->length();
	appendLinkChar(s, c + (Tuple) {+1,-1}, kNNW, "|");
	appendLinkChar(s, c + (Tuple) { 0, 0}, kESE, "_");
	if (len == s->length()) s->append(" ");

}

void Board::appendAfterRow(string *s, Tuple c) const {

	// -1, -1
	int len = s->length();
	appendLinkChar(s, c + (Tuple) {+1, -1}, kWNW, "\\");
	appendLinkChar(s, c + (Tuple) { 0, -1}, kNNW, "\\");
	if (len == s->length()) s->append(" ");

	//  0, -1
	len = s->length();
	appendLinkChar(s, c + (Tuple) {-1, -1}, kENE, "_");
	appendLinkChar(s, c + (Tuple) {+1, -1}, kWNW, "_");
	appendLinkChar(s, c, kSSW, "|");
	if (len == s->length()) appendLinkChar(s, c, kSSE, "|");
	if (len == s->length()) s->append(" ");

	// -1, -1
	len = s->length();
	appendLinkChar(s, c + (Tuple) {-1, -1}, kENE, "/");
	appendLinkChar(s, c + (Tuple) { 0, -1}, kNNE, "/");
	if (len == s->length()) s->append(" ");
}

void Board::undoFirstMove(Tuple c) {
	Cell *pCell = getCell(c);
	pCell->setColor(kEmpty);
	// initialize Candidates but not static blockerMap
	initializeCandidates(c, pCell, false);
	initializeLegalActions();
}

void Board::applyAction(int player, Action move) {

	Tuple c = { (int) move % getSize(), (int) move / getSize() };

	if (getMoveCounter() == 1) {
		if (move == getMoveOne()) {
			// second player swapped
			setSwapped(true);

			// undo first move (peg and legal actions)
			undoFirstMove(c);

			// turn move 90° clockwise
			move = (move % getSize()) * getSize()
					+ (getSize() - (move / getSize()) - 1);

			// get coordinates for move
			c = { (int) move % getSize(), (int) move / getSize() };

		} else {
			// not swapped => regular move
			// remove move #1 from legal moves
			removeLegalAction(kRedPlayer, getMoveOne());
			removeLegalAction(kBluePlayer, getMoveOne());
		}
	}


	setPegAndLinks(player, c);


	if (getMoveCounter() == 0) {
		// do not remove the move from legal actions but store it
		// because second player might want to swap, i.e. chose same move
		setMoveOne(move);
	} else {
		// other wise remove move from mLegalActions
		removeLegalAction(kRedPlayer, move);
		removeLegalAction(kBluePlayer, move);
	}

	incMoveCounter();

	// Update the predicted result and update mCurrentPlayer...
	updateResult(player, c);

}

void Board::setPegAndLinks(int player, Tuple c) {

	bool linkedToNeutral = false;
	bool linkedToStart = false;
	bool linkedToEnd = false;

	// set peg
	Cell *pCell = getCell(c);
	pCell->setColor(player);

	int dir=0;
	bool newLinks = false;
	// check all candidates (neigbors that are empty or have same color)
	for (int cand=1, dir=0; cand <= pCell->getCandidates(player) ; cand<<=1, dir++) {
		if (pCell->isCandidate(player, cand)) {

			Tuple n = pCell->getNeighbor(dir);	

			Cell *pTargetCell = getCell(pCell->getNeighbor(dir));
			if (pTargetCell->getColor() == kEmpty) {
				// pCell is not a candidate for pTargetCell anymore
				// (from opponent's perspective)
				pTargetCell->deleteCandidate(1-player, oppCand(cand));
			} else {
				// check if there are blocking links before setting link
				set<Link> *blockers = getBlockers((Link) {c, dir});
				bool blocked = false;
				for (auto &&bl : *blockers) {
					if (getCell(bl.first)->hasLink(bl.second)) {
						blocked = true;
						break;
					}
				}

				if (! blocked) {
					// we set the link, and set the flag that there is at least one new link
					pCell->setLink(dir);
					pTargetCell->setLink(oppDir(dir));

					newLinks = true;

					// check if cell we link to is linked to START border / END border
					if (pTargetCell->isLinkedToBorder(player, kStart)) {
						pCell->setLinkedToBorder(player, kStart);
						linkedToStart = true;
					} else if (pTargetCell->isLinkedToBorder(player, kEnd)) {
						pCell->setLinkedToBorder(player, kEnd);
						linkedToEnd = true;
					} else {
						linkedToNeutral = true;
					}
				} // not blocked
			} // is not empty
		} // is candidate
	} // candidate range

	//check if we need to explore further
	if (newLinks) {
		if (pCell->isLinkedToBorder(player, kStart) && linkedToNeutral) {
			// case: new cell is linked to START and linked to neutral cells
			// => explore neutral graph and add all its cells to START
			exploreLocalGraph(player, pCell, kStart);
		}
		if (pCell->isLinkedToBorder(player, kEnd) && linkedToNeutral) {
			// case: new cell is linked to END and linked to neutral cells
			// => explore neutral graph and add all its cells to END
			exploreLocalGraph(player, pCell, kEnd);
		}
	}

}

void Board::exploreLocalGraph(int player, Cell *pCell, enum Border border) {

	int dir=0;
	for (int link=1, dir=0; link <= pCell->getLinks(); link<<=1, dir++) {
		if (pCell->isLinked(link)) {
			Cell *pTargetCell = getCell(pCell->getNeighbor(dir));
			if (! pTargetCell->isLinkedToBorder(player, border)) {
				// linked neighbor is NOT yet member of PegSet
				// => add it and explore
				pTargetCell->setLinkedToBorder(player, border);
				exploreLocalGraph(player, pTargetCell, border);
			}
		}
	}
}


bool Board::coordsOnBorder(int player, Tuple c) const {

	if (player == kRedPlayer) {
		return ((c.second == 0 || c.second == getSize() - 1)
				&& (c.first > 0 && c.first < getSize() - 1));
	} else {
		return ((c.first == 0 || c.first == getSize() - 1)
				&& (c.second > 0 && c.second < getSize() - 1));
	}
}

bool Board::coordsOffBoard(Tuple c) const {

	return (c.second < 0 || c.second > getSize() - 1 ||
			c.first  < 0 || c.first >  getSize() - 1 ||
			// corner case
		   ((c.first  == 0 || c.first  == getSize() - 1) &&
		    (c.second == 0 || c.second == getSize() - 1)));
}


}  // namespace twixt
}  // namespace open_spiel

