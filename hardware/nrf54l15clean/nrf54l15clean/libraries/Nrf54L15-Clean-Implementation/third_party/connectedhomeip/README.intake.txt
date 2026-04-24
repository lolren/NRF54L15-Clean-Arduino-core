This directory is the staged connectedhomeip path for future Matter work.

Current state:

- a minimal upstream header/support/error/key/time/hex/thread-dataset seed is
  imported from connectedhomeip commit
  337f8f54b4f0813681664e5b179dc3e16fdd14a0
- that seed is only large enough for hidden-seam compile smoke against a few
  upstream core headers, staged upstream support implementation units, and
  staged upstream core error / key-id implementation units through repo-owned
  config / CodeUtils shims
- it is not a full upstream scaffold

To refresh this minimal seed, use:

  scripts/import_connectedhomeip_scaffold.sh <connectedhomeip-ref>

The presence of this directory still does not mean a compileable Matter target
exists.
