return {
  {
    id = "additive_expression",
    rule = {
      {
        nterm = { { num = 0, id = "multiplicative_expression" } },
        rhs = { 0 },
        -- No action for this rule
      },
      {
        nterm = {
          { num = 0, id = "additive_expression" },
          { num = 1, id = "multiplicative_expression" }
        },
        token = { { num = 2, id = "PLUS" } },
        rhs = { 0, 2, 1 },
        action = [[
          // C code for addition
          $$ = $1 + $3;
        ]]
      },
      {
        nterm = {
          { num = 0, id = "additive_expression" },
          { num = 1, id = "multiplicative_expression" }
        },
        token = { { num = 2, id = "DASH" } },
        rhs = { 0, 2, 1 },
        action = [[
          // C code for subtraction
          $$ = $1 - $3;
        ]]
      }
    }
  }
}