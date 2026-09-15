### Requirements

libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel

v_oos:

s >= 2*sa: t = 2*(vmax/a) + vmax*(s - 2*sa) = vmax*(2/a + (s-2*sa))
s < 2*sa: t = 2*sqrt(2*s/a)

### todo

- newly spawned asteroids are not synched with client somehow
- refactor the opool and ecs sync for nicer code. It works for now but is ugly
- add sector newly spawned entites vector so these can be initially put into broadphase test (otherwise they wont collide if not moved)
- wont see projectiles in client if lifetime is sufficiently short, never a valid interpolation data set
- rework UI with nice templates