
<!-- README.md is generated from README.Rmd. Please edit that file -->

# slide

<!-- badges: start -->

[![Travis build
status](https://travis-ci.org/DavisVaughan/slide.svg?branch=master)](https://travis-ci.org/DavisVaughan/slide)
[![Codecov test
coverage](https://codecov.io/gh/DavisVaughan/slide/branch/master/graph/badge.svg)](https://codecov.io/gh/DavisVaughan/slide?branch=master)
<!-- badges: end -->

slide provides a family of general purpose “sliding window” functions.
The API is purposefully *very* similar to purrr, with functions such as
`slide()`, `slide_dbl()`, `slide2()` and `pslide()`. The goal of these
functions is usually to compute rolling averages, cumulative sums,
rolling regressions, or other “window” based computations.

There are 3 reasons to use slide:

  - Like `purrr::map()`, `slide()` is type stable, and also always
    returns a result with the same size as its input.

  - Unlike `map()`, with data frames `slide()` iterates *row wise*. This
    is consistent with the theory that backs `slide()`, but also makes
    it a generic row wise data frame iterator that solves a number of
    problems outside of sliding windows. It just happens to be a neat
    side effect of this API.

  - If you have ever needed to compute a rolling calculation *relative
    to an index*, then you might like `slide_index()`. This solves the
    problem of computing a “rolling mean over a 3 month window”, where
    the number of days in each month is irregular.

If you are new to slide, but are familiar with purrr, I would encourage
you to start with the documentation and examples for
[`?slide`](https://davisvaughan.github.io/slide/reference/slide.html)
and
[`?slide_index`](https://davisvaughan.github.io/slide/reference/slide_index.html).

## Installation

slide is NOT yet on [CRAN](https://CRAN.R-project.org).

You can install the development version from
[GitHub](https://github.com/) with:

``` r
remotes::install_github("DavisVaughan/slide")
```

(Just a warning that this uses a custom version of vctrs, which will
soon become its development version).

## Examples

The [help page for
`slide()`](https://davisvaughan.github.io/slide/reference/slide.html)
has many examples, but here are a few:

``` r
library(slide)
```

The classic example would be to do a moving average. `slide()` handles
this with a combination of the `.before` and `.after` arguments, which
control the width of the window and the alignment.

``` r
# Moving average (Aligned right)
# "The current element + 2 elements before"
slide_dbl(1:5, ~mean(.x), .before = 2)
#> [1] 1.0 1.5 2.0 3.0 4.0

# Align left
# "The current element + 2 elements after"
slide_dbl(1:5, ~mean(.x), .after = 2)
#> [1] 2.0 3.0 4.0 4.5 5.0

# Center aligned
# "The current element + 1 element before + 1 element after"
slide_dbl(1:5, ~mean(.x), .before = 1, .after = 1)
#> [1] 1.5 2.0 3.0 4.0 4.5
```

With `unbounded()`, you can do a “cumulative slide” to compute
cumulative expressions. I think of this as saying “give me everything
before the current element.”

``` r
slide(1:4, ~.x, .before = unbounded())
#> [[1]]
#> [1] 1
#> 
#> [[2]]
#> [1] 1 2
#> 
#> [[3]]
#> [1] 1 2 3
#> 
#> [[4]]
#> [1] 1 2 3 4
```

With `.complete`, you can decide whether or not `.f` should be evaluated
on incomplete windows. In the following example, the requested window
size is 3, but the first two results are computed on windows of size 1
and 2 because partial results are allowed by default. When `.complete`
is set to `TRUE`, the first two results are not computed.

``` r
slide(1:4, ~.x, .before = 2)
#> [[1]]
#> [1] 1
#> 
#> [[2]]
#> [1] 1 2
#> 
#> [[3]]
#> [1] 1 2 3
#> 
#> [[4]]
#> [1] 2 3 4

slide(1:4, ~.x, .before = 2, .complete = TRUE)
#> [[1]]
#> NULL
#> 
#> [[2]]
#> NULL
#> 
#> [[3]]
#> [1] 1 2 3
#> 
#> [[4]]
#> [1] 2 3 4
```

## Data frames

Unlike `purrr::map()`, `slide()` iterates over data frames in a row wise
fashion. Interestingly this means the default of `slide()` becomes a
generic row wise iterator, with nice syntax for accessing data frame
columns.

``` r
cars <- mtcars[1:4,]

slide(cars, ~.x)
#> [[1]]
#>           mpg cyl disp  hp drat   wt  qsec vs am gear carb
#> Mazda RX4  21   6  160 110  3.9 2.62 16.46  0  1    4    4
#> 
#> [[2]]
#>               mpg cyl disp  hp drat    wt  qsec vs am gear carb
#> Mazda RX4 Wag  21   6  160 110  3.9 2.875 17.02  0  1    4    4
#> 
#> [[3]]
#>             mpg cyl disp hp drat   wt  qsec vs am gear carb
#> Datsun 710 22.8   4  108 93 3.85 2.32 18.61  1  1    4    1
#> 
#> [[4]]
#>                 mpg cyl disp  hp drat    wt  qsec vs am gear carb
#> Hornet 4 Drive 21.4   6  258 110 3.08 3.215 19.44  1  0    3    1

slide_dbl(cars, ~.x$mpg + .x$drat)
#> [1] 24.90 24.90 26.65 24.48
```

This makes rolling regressions trivial\!

``` r
library(tibble)
set.seed(123)

df <- tibble(
  y = rnorm(100),
  x = rnorm(100)
)

# Window size of 20 rows
# The current row + 19 before
# (see slide_index() for how to do this relative to a date vector!)
df$regressions <- slide(df, ~lm(y ~ x, data = .x), .before = 19, .complete = TRUE)

df[15:25,]
#> # A tibble: 11 x 3
#>         y      x regressions
#>     <dbl>  <dbl> <list>     
#>  1 -0.556  0.519 <NULL>     
#>  2  1.79   0.301 <NULL>     
#>  3  0.498  0.106 <NULL>     
#>  4 -1.97  -0.641 <NULL>     
#>  5  0.701 -0.850 <NULL>     
#>  6 -0.473 -1.02  <lm>       
#>  7 -1.07   0.118 <lm>       
#>  8 -0.218 -0.947 <lm>       
#>  9 -1.03  -0.491 <lm>       
#> 10 -0.729 -0.256 <lm>       
#> 11 -0.625  1.84  <lm>
```

## Index sliding

In many business settings, the value you want to compute is tied to some
*index*, like a date vector. In these cases, you’ll probably want to
compute sliding windows relative to the index, and not using the fixed
window that `slide()` provides. You can use `slide_index()` to pass in
both `.x` and an index, `.i`, and the window will be calculated relative
to that index.

Here, when computing a “2 day window”, you probably don’t want
`"2019-08-16"` and `"2019-08-18"` to be grouped together. `slide()` has
no concept of an index, so when you specify a window size of 2, it will
group these two together. `slide_index()`, on the other hand, will do
the right thing.

``` r
x <- 1:3
i <- as.Date(c("2019-08-15", "2019-08-16", "2019-08-18"))

# slide() has no concept of an "index"
slide(x, ~.x, .before = 1)
#> [[1]]
#> [1] 1
#> 
#> [[2]]
#> [1] 1 2
#> 
#> [[3]]
#> [1] 2 3

# "index aware"
slide_index(x, i, ~.x, .before = 1)
#> [[1]]
#> [1] 1
#> 
#> [[2]]
#> [1] 1 2
#> 
#> [[3]]
#> [1] 3
```

Essentially what happens is that when we get to `"2019-08-18"`, it
“looks backwards” 1 day to set a window boundary at `"2019-08-17"`.
Since the date at position 2, `"2019-08-16"`, is before `"2019-08-17"`,
it is not included.

Powerfully, you can pass through any object to `.before` that computes a
value from `.i - .before`. This means that you could also have used a
lubridate period object (which gets even more interesting when you use
`weeks()` or `months()`):

``` r
slide_index(x, i, ~.x, .before = lubridate::days(1))
#> [[1]]
#> [1] 1
#> 
#> [[2]]
#> [1] 1 2
#> 
#> [[3]]
#> [1] 3
```

## Inspiration

This package is inspired heavily by SQL’s window functions. The API is
similar, but more general because you can iterate over any kind of R
object.

There have been multiple attempts at creating sliding window functions
(I personally created `rollify()`, and worked a little bit on
`tsibble::slide()` with [Earo Wang](https://github.com/earowang)).

  - `zoo::rollapply()`
  - `tibbletime::rollify()`
  - `tsibble::slide()`

I believe that slide is the next iteration of these. There are a few
reasons for this:

  - To me, the API is more intuitive, and is more flexible because
    `.before` and `.after` let you completely control the entry point
    (as opposed to fixed entry points like `"center"`, `"left"`, etc.

  - It is objectively faster because it is written purely in C.

  - With `slide_vec()` you can return any kind of object, and are not
    limited to the suffixed versions: `_dbl`, `_int`, etc.

  - It iterates rowwise over data frames, consistent with the vctrs
    framework.

  - I believe it is overall more consistent, backed by a theory that can
    always justify the sliding window generated by any combination of
    the parameters.

Earo and I have spoken, and we have mututally agreed that it would be
best to deprecate `tsibble::slide()` in favor of `slide::slide()`.

Additionally, [data.table](https://github.com/Rdatatable/data.table)’s
non-equi joins have been pretty much the only solution to the problem
that `slide_index()` tries to solve. Their solution is robust and quite
fast, and has been a nice benchmark for slide. slide is trying to solve
a much narrower problem, so the API here is more focused.

## Performance

In terms of performance, be aware that any specialized package that
shifts the function calls to C are going to be faster than slide. For
example, `RcppRoll::roll_mean()` computes the rolling mean *at the C
level*, which is bound to be faster. The purpose of slide is to be
*general purpose*, while still being as fast as possible. This means
that it can be used for more abstract things, like rolling regressions,
or any other custom function that you want to use in a rolling fashion.

Otherwise, like `purrr::map()`, `slide()` is optimized in C to be as
fast as possible, getting out of the way as quickly as it can so the
main overhead are the `.f` calls.

## References

I’ve found the following references very useful to understand more about
window functions:

  - [Postgres SQL
    documentation](https://www.postgresql.org/docs/9.1/sql-expressions.html#SYNTAX-WINDOW-FUNCTIONS)

  - [dbplyr window function
    vignette](https://dbplyr.tidyverse.org/articles/translation-function.html#window-functions)

  - [SQLite documentation - with a
    flowchart](https://www.sqlite.org/windowfunctions.html)

  - [Vertica Rows vs Range
    discussion](https://www.vertica.com/docs/9.2.x/HTML/Content/Authoring/SQLReferenceManual/Functions/Analytic/window_frame_clause.htm?origin_team=T02V9CHFH#ROWSversusRANGE)