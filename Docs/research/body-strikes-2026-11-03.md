# A hand on the body, 2026-11-03

From 0.17.0 the keys below the lowest string were a hand striking the body,
where from 0.10.0 to 0.16.0 they were a moved ji that no koto has
([README](../../README.md), Known gaps). From 0.18.0 only the twelve under it
(D2 to C♯3 on both presets) are: twelve hits of three hands at different
places, and the keys below are free, kept for the playing techniques still to
come. This is the first of those techniques, staged one at a time; the owner's
condition was that every technique and effect be physically simulated. Nothing
here is a sample, and no recording of a strike was compared.

## What players do

Pornprasit, "Playing Techniques of New Pieces of Koto Music in Japanese Urban
Culture", *Journal of Urban Culture Research* 1 (2010 issue), pp. 178-188,
doi 10.14456/jucr.2010.12, section 2.9, "Hitting the Koto Body with the Palms
to Create Special Sounds": the left or right hand strikes the body or back, and
"the resulting sound is comparable to that of striking a large, open wooden box
with a drumstick". Its figures 13 and 14 show a strike on the outermost edge of
the body next to the first string and one underneath the body next to the
player. The article describes the new music of the 20th century, not the
traditional repertoire, and gives no force, contact time or place measurement.

[New Music USA](https://newmusicusa.org/?p=273754) credits Kazue Sawai with
pioneering "playing on the entire body of the instrument" among other extended
techniques. A commercial sample library lists the same strike among its
articulations ("hand strikes on the wood body"); that is a product, not
evidence.

## The model

The body is the engine's own: 356 modes of a paulownia box, built from its
beams' bending and twist, its walls' modes and the air in its cavity and at its
holes (`designBody`). Each mode already carries its outward displacement at
3,602 points of the surface for the microphones. A strike reads that displacement
where the hand lands, so the force goes into exactly the modes that move there
and nowhere else, and the strings ring in sympathy through the ji as they do
under any other body motion.

- **Places.** Three surfaces: the top's far edge beside the first string, the
  near side wall, and the back's middle (Pornprasit's figures 13 and 14, and
  the back). A place is a surface and a fraction of the body's length from the
  head, between 22% and 76%, where the body answers best, and no hand strikes
  one surface at two places that mirror each other about the middle, where the
  body answers alike. The
  hand's patch is the mean over five stations (7.6 cm of the 1.83 m body) by
  three cells across the top (7.2 cm), the side's full height, or four across
  the back (9.6 cm): a palm, for all three hands.
- **Hands and keys.** The twelve keys under the lowest string are twelve hits,
  four to each hand in order, so the keys climb from deep to sharp: the palm on
  D2-F2, the knuckle on F♯2-A2 and the rap on A♯2-C♯3. Each hand strikes the
  top twice, the near side once and the back once (`Source/DSP/AcukotoStrikes.h`
  has the table). Keys further down are free: they sound nothing.
- **Contact.** A mass presses into the surface through a Hertzian spring,
  `F = k d^1.5`, with Hunt and Crossley's damping,
  `F = k d^1.5 (1 + 1.5 (1 - e) / v0 * dd/dt)`, `e` the restitution and `v0` the
  speed of arrival. The compression `d` closes at the hand's speed plus the
  surface's, both inward, so the body's recoil shortens the contact. The force
  goes into the modes as a generalized force, `-F` times the surface's
  displacement per unit of each mode, and the modes' velocity there comes back.
  The contact ends when the compression returns to zero. With no hand in
  contact it costs one branch per sample.
- **Velocity.** The key's velocity sets the speed of arrival, 0.02 to 0.5 m/s,
  geometrically, times the hit's touch.
- **Touch.** The hits stand 18.8 dB apart at one velocity without it (the
  backs loudest, the palm on the near side quietest), so each carries a scale
  on its arrival speed taking 80% of the way to the strings' mean level, as the
  strings' touches do. `AcukotoLevels` measures it and `Acukoto.Levels` checks
  it ([validation](../validation/body-strikes-2026-11-03.md)).

## What is estimated

The palm's mass (0.3 kg, a hand and the forearm behind it), stiffness
(1.36e6 N/m^1.5) and the restitution (0.4), the knuckle's (0.1 kg, 5.9e6) and
the rap's (0.05 kg, 2.4e7) and the speed range are estimates; no source gives
them for a hand on a koto. The palm's stiffness puts a 0.3 kg hand arriving at
1 m/s in contact for about 7 ms with a peak force near 150 N, the scale of a
palm's slap; a knuckle is lighter and stiffer, and its contact shorter and
brighter, and a rap more so. A hand's real speeds are higher than the range:
at 1 m/s the body stands 15 to 25 dB over a plucked string's level, so the
range is scaled to put velocity 0.7 about as loud as a note. The owner heard
the three hands as a lettered set and chose all three
([decisions](../decisions.md)); they are three defensible hands, not three
measured ones. Which hand and which place each key carries, and the touches,
are the engine's layout.

## Not modelled

The hand's own tissue (a pad that stiffens under load, the arm behind it), a
contact patch that differs with the hand (a knuckle or a rap meets a smaller
area than a palm, and here all three share a palm's), a second strike
overlapping the first (a new strike replaces a contact still running), a hand
that lingers on the surface and damps it, and the strings: a strike beside the
strings does not touch them. The touches even the hits at velocity 0.7; at 0.3
they stand 8.1 dB apart and at 1.0 2.9 dB
([validation](../validation/body-strikes-2026-11-03.md)).
