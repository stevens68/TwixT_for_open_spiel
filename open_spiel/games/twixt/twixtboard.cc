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
	return (dir + COMPASS_COUNT / 2) % COMPASS_COUNT;
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
			{{ 0,  1}, Compass::ENE },
			{{-1,  0}, Compass::ENE },

			{{ 0,  2}, Compass::ESE },
			{{ 0,  1}, Compass::ESE },
			{{-1,  2}, Compass::ESE },
			{{-1,  1}, Compass::ESE },

			{{ 0,  1}, Compass::SSE },
			{{ 0,  2}, Compass::SSE },
			{{ 0,  3}, Compass::SSE }
	   }
	},
	// ENE
	{
	   {2,  1},
	   {
			{{ 0, -1}, Compass::NNE },
			{{ 1,  0}, Compass::NNE },

			{{-1,  1}, Compass::ESE },
			{{ 0,  1}, Compass::ESE },
			{{ 1,  1}, Compass::ESE },

			{{ 0,  1}, Compass::SSE },
			{{ 0,  2}, Compass::SSE },
			{{ 1,  1}, Compass::SSE },
			{{ 1,  2}, Compass::SSE }
	   }
	},
	// ESE
	{
	   { 2, -1},
	   {
			{{ 0, -1}, Compass::NNE },
			{{ 1, -1}, Compass::NNE },
			{{ 0, -2}, Compass::NNE },
			{{ 1, -2}, Compass::NNE },

			{{-1, -1}, Compass::ENE },
			{{ 0, -1}, Compass::ENE },
			{{ 1, -1}, Compass::ENE },

			{{ 0,  1}, Compass::SSE },
			{{ 1,  0}, Compass::SSE }
	   }
	},
	// SSE
	{
	   { 1, -2},
	   {
			{{ 0, -1}, Compass::NNE },
			{{ 0, -2}, Compass::NNE },
			{{ 0, -3}, Compass::NNE },

			{{-1, -1}, Compass::ENE },
			{{ 0, -1}, Compass::ENE },
			{{-1, -2}, Compass::ENE },
			{{ 0, -2}, Compass::ENE },

			{{-1,  0}, Compass::ESE },
			{{ 0, -1}, Compass::ESE }
	   }
	},
	// SSW
	{
	   {-1, -2},
	   {
			{{-1, -1}, Compass::ENE },
			{{-2, -2}, Compass::ENE },

			{{-2,  0}, Compass::ESE },
			{{-1,  0}, Compass::ESE },
			{{-2, -1}, Compass::ESE },
			{{-1, -1}, Compass::ESE },

			{{-1,  1}, Compass::SSE },
			{{-1,  0}, Compass::SSE },
			{{-1, -1}, Compass::SSE }
	   }
	},
	// WSW
	{
	   {-2, -1},
	   {
			{{-2, -2}, Compass::NNE },
			{{-1, -1}, Compass::NNE },

			{{-3,  0}, Compass::ESE },
			{{-2,  0}, Compass::ESE },
			{{-1,  0}, Compass::ESE },

			{{-2,  1}, Compass::SSE },
			{{-1,  1}, Compass::SSE },
			{{-2,  0}, Compass::SSE },
			{{-1,  0}, Compass::SSE }
	   }
	},
	// WNW
	{
	   {-2, 1},
	   {
			{{-2,  0}, Compass::NNE },
			{{-1,  0}, Compass::NNE },
			{{-2, -1}, Compass::NNE },
			{{-1, -1}, Compass::NNE },

			{{-3,  0}, Compass::ENE },
			{{-2,  0}, Compass::ENE },
			{{-1,  0}, Compass::ENE },

			{{-2,  2}, Compass::SSE },
			{{-1,  1}, Compass::SSE }
	   }
	},
	// NNW
	{
	   {-1, 2},
	   {
			{{-1,  1}, Compass::NNE },
			{{-1,  0}, Compass::NNE },
			{{-1, -1}, Compass::NNE },

			{{-2,  1}, Compass::ENE },
			{{-1,  1}, Compass::ENE },
			{{-2,  0}, Compass::ENE },
			{{-1,  0}, Compass::ENE },

			{{-2,  2}, Compass::ESE },
			{{-1,  1}, Compass::ESE }
	   }
	}

};



Board::Board(int size, bool ansiColorOutput) {
	setSize(size);
	setAnsiColorOutput(ansiColorOutput);


	initializeCells(true);
	initializeLegalActions();
	initializeTensor();
}

void Board::initializeTensor() {

	int planeArea = getSize() * (getSize()-2);
	// PLAYER_RED: 10 planes: 0.0, 1 plane 0.0
	// PLAYER_BLUE: 10 planes: 0.0, 1 plane 1.0

	mTensor[PLAYER_RED].resize(kNumPlanes * planeArea);
	mTensor[PLAYER_BLUE].resize(kNumPlanes * planeArea);
	for (int i=0; i < planeArea; i++) {
		mTensor[PLAYER_BLUE][(kNumPlanes-1) * planeArea + i] = 1.0;
	}

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
	bool connectedToStart = getCell(c)->isLinkedToBorder(player, Border::START);
	bool connectedToEnd = getCell(c)->isLinkedToBorder(player, Border::END);
	if (connectedToStart && connectedToEnd) {
		// peg is linked to both boarder lines
		setResult(player == PLAYER_RED ? Result::RED_WON : Result::BLUE_WON);
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
		setResult(Result::DRAW);
		return;
	}
}

void Board::initializeCells(bool initBlockerMap) {

	mCell.resize(getSize());
	clearBlocker();

	// initialize board with color (empty or overboard)
	for (int x = 0; x < getSize(); x++) {
		//adjust dim 2 of board
		mCell[x].resize(getSize());
		for (int y = 0; y < getSize(); y++) {
			// links
			Tuple c = {x, y};
			Cell *pCell = getCell(c);
			// set color to EMPTY or OVERBOARD
			if (coordsOffBoard(c)) {
				pCell->setColor(OVERBOARD);
			} else { // regular board
				pCell->setColor(EMPTY);
				if (x == 0) {
					pCell->setLinkedToBorder(PLAYER_BLUE, Border::START);
				} else if (x == getSize()-1) {
					pCell->setLinkedToBorder(PLAYER_BLUE, Border::END);
				} else if (y == 0) {
					pCell->setLinkedToBorder(PLAYER_RED, Border::START);
				} else if (y == getSize()-1) {
					pCell->setLinkedToBorder(PLAYER_RED, Border::END);
				}

				initializeCandidates(c, pCell, initBlockerMap);
			}
		}
	}
}

void Board::initializeCandidates(Tuple c, Cell *pCell, bool initBlockerMap) {

	for (int dir = 0; dir < COMPASS_COUNT; dir++) {
		LinkDescriptor *ld = &(kLinkDescriptorTable[dir]);
		Tuple tc = c + ld->offsets;
		if (! coordsOffBoard(tc)) {
			if (initBlockerMap) {
				initializeBlockerMap(c, dir, ld);
			}
			pCell->setNeighbor(dir, tc);
			Cell *pTargetCell = getCell(tc);
			if (! (coordsOnBorder(PLAYER_RED, c) && coordsOnBorder(PLAYER_BLUE, tc)) &&
				! (coordsOnBorder(PLAYER_BLUE, c) && coordsOnBorder(PLAYER_RED, tc))) {
					pCell->setCandidate(PLAYER_RED, dir);
					pCell->setCandidate(PLAYER_BLUE, dir);
			}
		}
	}
}

void Board::initializeLegalActions() {

	for (int player = PLAYER_RED; player < PLAYER_COUNT; player++) {
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
	case Result::OPEN:
		break;
	case Result::RED_WON:
		s.append("[X has won]");
		break;
	case Result::BLUE_WON:
		s.append("[O has won]");
		break;
	case Result::DRAW:
		s.append("[draw]");
	default:
		break;
	}

	return s;
}

string Board::actionToString(Action move) const {

	string s = "";
	s += char(int('A') + move % getSize());
	s.append(to_string(getSize() - move / getSize()));
	return s;
}

void Board::createTensor(int player, vector<double> *values) const {

	*values = mTensor[player];
}


void Board::appendLinkChar(string *s, Tuple c, enum Compass dir, string linkChar) const {
	if (! coordsOffBoard(c) && getConstCell(c)->hasLink(dir)) {
		if (getConstCell(c)->getColor() == PLAYER_RED) {
			appendColorString(s, kAnsiRed, linkChar);
		} else if (getConstCell(c)->getColor() == PLAYER_BLUE) {
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
	if (getConstCell(c)->getColor() == PLAYER_RED) {
		// X
		appendColorString(s, kAnsiRed, "X");
	} else if (getConstCell(c)->getColor() == PLAYER_BLUE) {
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
	appendLinkChar(s, c + (Tuple) {-1, 0}, Compass::ENE, "/");
	appendLinkChar(s, c + (Tuple) {-1,-1}, Compass::NNE, "/");
	appendLinkChar(s, c + (Tuple) { 0, 0}, Compass::WNW, "_");
	if (len == s->length()) s->append(" ");

	//  0, +1
	len = s->length();
	appendLinkChar(s, c, Compass::NNE, "|");
	if (len == s->length())	appendLinkChar(s, c, Compass::NNW, "|");
	if (len == s->length()) s->append(" ");

	// +1, +1
	len = s->length();
	appendLinkChar(s, c + (Tuple) {+1, 0}, Compass::WNW, "\\");
	appendLinkChar(s, c + (Tuple) {+1,-1}, Compass::NNW, "\\");
	appendLinkChar(s, c + (Tuple) { 0, 0}, Compass::ENE, "_");
	if (len == s->length())	s->append(" ");

}

void Board::appendPegRow(string *s, Tuple c) const {

	// -1, 0
	int len = s->length();
	appendLinkChar(s, c + (Tuple) {-1,-1}, Compass::NNE, "|");
	appendLinkChar(s, c + (Tuple) { 0, 0}, Compass::WSW, "_");
	if (len == s->length()) s->append(" ");

	//  0,  0
	appendPegChar(s, c);

	// +1, 0
	len = s->length();
	appendLinkChar(s, c + (Tuple) {+1,-1}, Compass::NNW, "|");
	appendLinkChar(s, c + (Tuple) { 0, 0}, Compass::ESE, "_");
	if (len == s->length()) s->append(" ");

}

void Board::appendAfterRow(string *s, Tuple c) const {

	// -1, -1
	int len = s->length();
	appendLinkChar(s, c + (Tuple) {+1, -1}, Compass::WNW, "\\");
	appendLinkChar(s, c + (Tuple) { 0, -1}, Compass::NNW, "\\");
	if (len == s->length()) s->append(" ");

	//  0, -1
	len = s->length();
	appendLinkChar(s, c + (Tuple) {-1, -1}, Compass::ENE, "_");
	appendLinkChar(s, c + (Tuple) {+1, -1}, Compass::WNW, "_");
	appendLinkChar(s, c, Compass::SSW, "|");
	if (len == s->length()) appendLinkChar(s, c, Compass::SSE, "|");
	if (len == s->length()) s->append(" ");

	// -1, -1
	len = s->length();
	appendLinkChar(s, c + (Tuple) {-1, -1}, Compass::ENE, "/");
	appendLinkChar(s, c + (Tuple) { 0, -1}, Compass::NNE, "/");
	if (len == s->length()) s->append(" ");
}

void Board::undoFirstMove(Tuple c) {
	Cell *pCell = getCell(c);
	pCell->setColor(EMPTY);
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
			removeLegalAction(PLAYER_RED, getMoveOne());
			removeLegalAction(PLAYER_BLUE, getMoveOne());
		}
	}


	setPegAndLinks(player, c);


	if (getMoveCounter() == 0) {
		// do not remove the move from legal actions but store it
		// because second player might want to swap, i.e. chose same move
		setMoveOne(move);
	} else {
		// other wise remove move from mLegalActions
		removeLegalAction(PLAYER_RED, move);
		removeLegalAction(PLAYER_BLUE, move);
	}

	incMoveCounter();

	// Update the predicted result and update mCurrentPlayer...
	updateResult(player, c);

}

void Board::updatePegOnTensor(int player, Tuple c) {

	int index;

	if (player == PLAYER_RED) {
		index = (c.first-1) * getSize() + c.second;
	} else {
		index = getSize() * (getSize()-2) + (c.second-1) * getSize() + c.first;
	}
	mTensor[PLAYER_RED][index] = 1.0;
	mTensor[PLAYER_BLUE][index] = 1.0;
}

void Board::updateLinkOnTensor(int player, Tuple c, int dir) {

	int index;

	if (player == PLAYER_RED) {
		index = (2 + 2*dir) * getSize() * (getSize()-2) + (c.first-1) * getSize() + c.second;
	} else {
		index = (2 + 2*dir + 1) * getSize() * (getSize()-2) + (c.second-1) * getSize() + c.first;
	}
	mTensor[PLAYER_RED][index] = 1.0;
	mTensor[PLAYER_BLUE][index] = 1.0;
}

void Board::setPegAndLinks(int player, Tuple c) {

	bool linkedToNeutral = false;
	bool linkedToStart = false;
	bool linkedToEnd = false;

	// set peg
	Cell *pCell = getCell(c);
	pCell->setColor(player);
	updatePegOnTensor(player, c);

	int dir=0;
	bool newLinks = false;
	// check all candidates (neigbors that are empty or have same color)
	for (int cand=1, dir=0; cand <= pCell->getCandidates(player) ; cand<<=1, dir++) {
		if (pCell->isCandidate(player, cand)) {
			Cell *pTargetCell = getCell(pCell->getNeighbor(dir));
			if (pTargetCell->getColor() == EMPTY) {
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
					// set link on Tensor
					if (dir < COMPASS_COUNT / 2) {
						updateLinkOnTensor(player, c, dir);
					} else {
						updateLinkOnTensor(player, pCell->getNeighbor(dir), oppDir(dir));
					}

					newLinks = true;

					// check if cell we link to is linked to START border / END border
					if (pTargetCell->isLinkedToBorder(player, Border::START)) {
						pCell->setLinkedToBorder(player, Border::START);
						linkedToStart = true;
					} else if (pTargetCell->isLinkedToBorder(player, Border::END)) {
						pCell->setLinkedToBorder(player, Border::END);
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
		if (pCell->isLinkedToBorder(player, Border::START) && linkedToNeutral) {
			// case: new cell is linked to START and linked to neutral cells
			// => explore neutral graph and add all its cells to START
			exploreLocalGraph(player, pCell, Border::START);
		}
		if (pCell->isLinkedToBorder(player, Border::END) && linkedToNeutral) {
			// case: new cell is linked to END and linked to neutral cells
			// => explore neutral graph and add all its cells to END
			exploreLocalGraph(player, pCell, Border::END);
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

	if (player == PLAYER_RED) {
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

