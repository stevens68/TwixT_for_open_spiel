# TwixT_for_open_spiel

C++ implementation of the board game [TwixT](https://en.wikipedia.org/wiki/TwixT) for deepmind's framework [open_spiel](https://github.com/deepmind/open_spiel).

![TwixT board](https://github.com/stevens68/TwixT_for_open_spiel/blob/master/pics/12x12game.JPG "TwixT board")

## Installation

* Clone deepmind's open_spiel [repository](https://github.com/deepmind/open_spiel) and follow the installation instructions
* Clone this repository and copy the files `TwixT_for_open_spiel/open_spiel/games/twixt*` into `open_spiel/open_spiel/games`
* Edit `open_spiel/open_spiel/games/CMakeLists.txt` and add the following lines
```
...
twixt.cc
twixt.h
twixt/twixtboard.cc
twixt/twixtboard.h
twixt/twixtcell.h 
...

...
add_executable(twixt_test twixt_test.cc ${OPEN_SPIEL_OBJECTS}
               $<TARGET_OBJECTS:tests>)
add_test(twixt_test twixt_test)
...
```
* Edit `open_spiel/open_spiel/python/tests/pyspiel_test.py` and add `"twixt"` to the list of games.
* Copy file `TwixT_for_open_spiel/open_spiel/integration_tests/playthroughs/twixt.txt` into `open_spiel/open_spiel/integration_tests/playthroughs/`
* Build the targets as described [here](https://github.com/deepmind/open_spiel/blob/master/docs/install.md)

## Examples

    ./build/examples/example --game=twixt
    
    ./build/examples/mcts_example --game="twixt(board_size=12)"
    
    ./build/examples/mcts_example --game=twixt -player1=mcts --player2=mcts --max_simulations=20000 --rollout_count=4 --verbose=true
    
    python ./open_spiel/python/examples/example.py --game=twixt\(board_size=12,ansi_color_output=False,discount=0.999\)


* board_size must be in [5..24], default=8
* discount must be in [0.0..1.0], default=1.0
* ansi_color_output must be True|False, default True


## Rules
* this is a paper-and-pencil variant of TwixT without link removal and without crossing of own links. 
* player 0 (x, red) has the top/bottom endlines, player 1 (o, blue) has the left/right endlines.
* If a player has no more legal moves left - when it's his turn - the game is a draw.
* player 1 (o, blue) can swap player 0's first move by choosing the same move again - unless it is on an endline.    
