#include "open_spiel/games/twixt/twixtcell.h"

using namespace std;

namespace open_spiel {
namespace twixt {


int Cell::getPeg() const {
	return mPeg;
}

void Cell::setPeg(int color) {
	mPeg = color;;
}

void Cell::clearLinks() {
	mLinks = 0;
}

void Cell::clearBlockedLinks() {
	mBlockedLinks = 0;
}

void Cell::setLink(int dir) {
	mLinks |= 1UL << dir;
}

void Cell::setBlocked(int dir) {
	mBlockedLinks |= 1UL << dir;
}

bool Cell::hasLink(int dir) const {
	return mLinks & 1UL << dir;
}

bool Cell::hasLinks() const {
	return mLinks > 0;
}

bool Cell::isBlocked(int dir) const {
	return mBlockedLinks & 1UL << dir;
}



}  // namespace twixt
}  // namespace open_spiel

