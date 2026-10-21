Based on swaylock 1.8.6, commit 44b82de635c3bc66b0093abd1cf8cc1c8b1b9c0f.
Upstream: https://github.com/swaywm/swaylock (MIT, see LICENSE).

XPWm replaces render.c with the shared XP LogonUI renderer, adds pointer support
for the submit arrow in seat.c, and adjusts the build to require PAM and produce
xpwm-lock. Authentication, password handling and ext-session-lock remain upstream.
The system /etc/pam.d/swaylock service is supplied by the distro swaylock package.
No test authentication bypass is included in the installed locker.
