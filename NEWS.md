# slider (development version)

* All `*_vec()` variants now maintain size stability when auto-simplifying
  (i.e. when `.ptype = NULL`) (#78).

* `hop()` and its variants no longer place the names of `.x` on the output.
  Because there is no _size_ guarantee on the output, the size of `.x` can
  be different than the size of the output, meaning that the names might not
  line up. This also affects `slide_period()`, which is implemented using
  a `hop()` variant (#75).

* With data frames containing row names, `slide()` and its variants now copy
  those row names onto the output. This is an implicit benefit from vctrs
  gaining better support for data frame row names.

# slider 0.1.2

* Updated to stay compatible with the latest version of vctrs.

# slider 0.1.1

* Fixed a "multiple definition" C issue when compiling with gcc10.

# slider 0.1.0

* Added a `NEWS.md` file to track changes to the package.
