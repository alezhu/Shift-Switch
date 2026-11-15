; Automatically extracted from VERSION file (single source of truth)
!if /FILEEXISTS "VERSION"
  !define _VERSION_FILE "VERSION"
!else if /FILEEXISTS "..\VERSION"
  !define _VERSION_FILE "..\VERSION"
!else
  !error "VERSION file not found"
!endif

!searchparse /file "${_VERSION_FILE}" "" VERSION_MAJOR "." VERSION_MINOR "." VERSION_PATCH
!define VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}"
!define VERSION_COMMA "${VERSION_MAJOR},${VERSION_MINOR},${VERSION_PATCH}"
!undef _VERSION_FILE
