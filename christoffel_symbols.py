import sympy as sp
from sympy import pprint

# Coordinates and parameter
c, t, r, th, ph, r_s = sp.symbols("c t r theta phi r_s", real=True)
coords = [t, r, th, ph]

# Schwarzschild factor
f = 1 - r_s / r

# Metric g_{mu nu}
g = sp.Matrix([[-c**2 * f, 0, 0, 0], [0, 1 / f, 0, 0], [0, 0, r**2, 0], [0, 0, 0, r**2 * sp.sin(th) ** 2]])

# Inverse metric g^{mu nu}
g_inv = sp.simplify(g.inv())

# Christoffel symbols: Gamma^mu_{nu rho}
n = 4
Gamma = [[[0 for _ in range(n)] for _ in range(n)] for _ in range(n)]

for mu in range(n):
    for nu in range(n):
        for rho in range(n):
            expr = 0
            for sigma in range(n):
                expr += g_inv[mu, sigma] * (
                    sp.diff(g[sigma, rho], coords[nu])
                    + sp.diff(g[sigma, nu], coords[rho])
                    - sp.diff(g[nu, rho], coords[sigma])
                )
            Gamma[mu][nu][rho] = sp.simplify(sp.Rational(1, 2) * expr)

# Print only nonzero components
names = ["t", "r", "theta", "phi"]

for mu in range(n):
    for nu in range(n):
        for rho in range(n):
            val = sp.simplify(Gamma[mu][nu][rho])
            if val != 0:
                print(f"Gamma^{names[mu]}_{{{names[nu]} {names[rho]}}} =", val)
                # print(f"\nGamma^{names[mu]}_{{{names[nu]} {names[rho]}}} =")
                # pprint(val)
