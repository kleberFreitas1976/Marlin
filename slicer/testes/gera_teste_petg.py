# Peca de teste do PETG: ventoinha (35-50%, pontes a 100%) e os recursos do Orca 2.5
# (arcos no preenchimento esparso, G-code de alta precisao). Gera um STL para fatiar
# no Orca com o perfil normal do PETG.
#
#   - bloco 20x20x12: preenchimento esparso (giroide no fatiamento) -> arcos G2/G3
#   - rampa de 45 graus saindo do bloco, sobre o ar -> saliencia com ventoinha
#   - dois pilares 6x6x15 e uma ponte de 12 mm entre eles -> ponte a 100%
#   - os pilares sozinhos acima do bloco tem camadas rapidas -> ventoinha no maximo
#   - base de 1,6 mm unindo tudo
#
# Uso: python gera_teste_petg.py <saida.stl>
import struct, sys

tri = []

def caixa(x0, y0, z0, x1, y1, z1):
    v = [(x, y, z) for x in (x0, x1) for y in (y0, y1) for z in (z0, z1)]
    # indices: bit2 = x, bit1 = y, bit0 = z; faces com normal para fora
    for a, b, c, d in ((0, 2, 6, 4), (1, 5, 7, 3), (0, 4, 5, 1), (2, 3, 7, 6), (0, 1, 3, 2), (4, 6, 7, 5)):
        tri.append((v[a], v[b], v[c]))
        tri.append((v[a], v[c], v[d]))

def prisma_rampa(x0, x1, y_face, z0, z1, avanco):
    """Rampa colada na face y=y_face do bloco: na altura z0 nao avanca, em z1 avanca 'avanco' em -Y"""
    A = (y_face, z0); B = (y_face, z1); C = (y_face - avanco, z1)    # triangulo no plano YZ
    for x, (p, q, r) in ((x0, (A, C, B)), (x1, (A, B, C))):        # tampas laterais
        tri.append(tuple((x, y, z) for y, z in (p, q, r)))
    for (ya, za), (yb, zb) in ((A, B), (B, C), (C, A)):              # tres faces retangulares
        p0, p1, p2, p3 = (x0, ya, za), (x0, yb, zb), (x1, yb, zb), (x1, ya, za)
        tri.append((p0, p1, p2))
        tri.append((p0, p2, p3))

BASE = 1.6
caixa(0, 0, 0, 52, 26, BASE)                          # base
caixa(2, 3, BASE, 22, 23, BASE + 12)                  # bloco do preenchimento
prisma_rampa(4, 20, 3, BASE + 2, BASE + 12, 10)       # rampa de 45 graus (10 de altura, 10 de avanco)
caixa(28, 10, BASE, 34, 16, BASE + 15)                # pilar 1
caixa(46, 10, BASE, 52, 16, BASE + 15)                # pilar 2 (vao de 12 mm)
caixa(28, 10, BASE + 15, 52, 16, BASE + 17)           # ponte

with open(sys.argv[1], "wb") as f:
    f.write(b"teste PETG ventoinha".ljust(80, b"\0") + struct.pack("<I", len(tri)))
    for t in tri:
        f.write(struct.pack("<3f", 0, 0, 0))
        for p in t:
            f.write(struct.pack("<3f", *p))
        f.write(b"\0\0")
print(f"{sys.argv[1]}: {len(tri)} triangulos")
