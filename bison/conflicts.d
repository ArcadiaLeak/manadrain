module bison.conflicts;
import bison;

bool[] conflicts;

bool[] shift_set;
bool[] lookahead_set;

void conflicts_solve() {
  symbol[] errors = new symbol[ntokens + 1];

  conflicts = new bool[nstates];
  shift_set = new bool[ntokens];
  lookahead_set = new bool[ntokens];

  foreach (s; states)
    set_conflicts(s, errors);
}

void set_conflicts(state s, symbol[] errors) {
  
}