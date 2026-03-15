coords = {t, r, \[Theta], \[CurlyPhi]};
rs; c;
f[r_] := 1 - rs/r;

g = DiagonalMatrix[{
  -c^2 f[r],
  1/f[r],
  r^2,
  r^2 Sin[\[Theta]]^2
}];
gInv = Inverse[g];

(* Christoffel symbols *)
\[CapitalGamma][\[Sigma]_, \[Mu]_, \[Nu]_] := \[CapitalGamma][\[Sigma], \[Mu], \[Nu]] = FullSimplify[
  Sum[
    1/2 gInv[[\[Sigma], \[Lambda]]] (
      D[g[[\[Lambda], \[Nu]]], coords[[\[Mu]]]] +
      D[g[[\[Lambda], \[Mu]]], coords[[\[Nu]]]] -
      D[g[[\[Mu], \[Nu]]], coords[[\[Lambda]]]]
    ),
    {\[Lambda], 4}
  ]
]

(* Print nonzero ones *)
Table[
  If[\[CapitalGamma][\[Sigma], \[Mu], \[Nu]] =!= 0,
    Print["\[CapitalGamma]^", coords[[\[Sigma]]], "_", coords[[\[Mu]]], coords[[\[Nu]]], " = ", \[CapitalGamma][\[Sigma], \[Mu], \[Nu]]]],
  {\[Sigma], 4}, {\[Mu], 4}, {\[Nu], 4}
];
