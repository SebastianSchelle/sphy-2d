### Requirements

libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel

v_oos:

s >= 2*sa: t = 2*(vmax/a) + vmax*(s - 2*sa) = vmax*(2/a + (s-2*sa))
s < 2*sa: t = 2*sqrt(2*s/a)

### todo
[ ] throw out stupid ai cmake shit and make it simple.
    e.g. put ai stuff or systems directly into server dir, it shouldn't be shared anyways