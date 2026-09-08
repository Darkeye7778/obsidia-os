# Shell application identity

The desktop groups windows as applications, while `displayd` continues to own
individual window state. A window's owner PID comes from the authenticated IPC
sender. The desktop resolves that PID through the generic process-info API and
associates the resulting bounded executable image name with the trusted built-in
application metadata registry. Known images use their stable application ID;
unknown images use the authenticated image name and a generic presentation.

Window titles are bounded application-provided presentation metadata. They are
never used as application identity or as an authorization input.

Pinned applications, desktop shortcuts, and running application groups are
separate layout concepts. Pinning retains a launch affordance but never starts an
application. Settings-write inheritance is granted only while launching trusted
metadata entries that declare that capability.

The launcher is desktop-shell UI. The authenticated root session owns one
bounded overlay surface which `displayd` composites above application windows
and below the cursor. Ordinary clients cannot create or configure the overlay,
and neither the overlay nor application surfaces receive final-display PRESENT
authority.
