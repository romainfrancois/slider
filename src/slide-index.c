#include "slide.h"
#include "slide-vctrs.h"
#include "utils.h"
#include "compare.h"
#include <vctrs.h>

// -----------------------------------------------------------------------------
// All defined below

static void check_starts_not_past_stops(SEXP starts, SEXP stops);

static void compute_window_sizes(int* window_sizes, SEXP x, int n);
static void compute_window_starts(int* window_starts, int* window_sizes, int n);
static void compute_window_stops(int* window_stops, int* window_sizes, int* window_starts, int n);

static int adjust_iteration_min(int iteration_min, SEXP range, SEXP i, int size);
static int adjust_iteration_max(int iteration_max, SEXP range, SEXP i, int size);

static int locate_window_start_index(SEXP i, SEXP start, int size, SEXP* p_last_start_position);
static int locate_window_stop_index(SEXP i, SEXP stop, int size, SEXP* p_last_stop_position);

// -----------------------------------------------------------------------------

SEXP slide_index_core_impl(SEXP x,
                           SEXP i,
                           SEXP starts,
                           SEXP stops,
                           SEXP out_indices,
                           SEXP f_call,
                           SEXP ptype,
                           SEXP env,
                           SEXP params) {

  int type = r_scalar_int_get(r_lst_get(params, 0));
  bool complete = r_scalar_lgl_get(r_lst_get(params, 1));
  bool before_unbounded = r_scalar_lgl_get(r_lst_get(params, 2));
  bool after_unbounded = r_scalar_lgl_get(r_lst_get(params, 3));
  bool constrain = r_scalar_lgl_get(r_lst_get(params, 4));
  int size = r_scalar_int_get(r_lst_get(params, 5));

  int size_i = vec_size(i);

  if (!before_unbounded && !after_unbounded) {
    check_starts_not_past_stops(starts, stops);
  }

  int iteration_min = 1;
  int iteration_max = size_i;

  // Iteration adjustment
  if (complete) {
    if (!before_unbounded) {
      iteration_min = adjust_iteration_min(iteration_min, starts, i, size_i);
    }
    if (!after_unbounded) {
      iteration_max = adjust_iteration_max(iteration_max, stops, i, size_i);
    }
  } else {
    if (!before_unbounded) {
      iteration_max = adjust_iteration_max(iteration_max, starts, i, size_i);
    }
    if (!after_unbounded) {
      iteration_min = adjust_iteration_min(iteration_min, stops, i, size_i);
    }
  }

  PROTECT_INDEX out_prot_idx;
  SEXP out = vec_init(ptype, size);
  PROTECT_WITH_INDEX(out, &out_prot_idx);
  out = vec_proxy(out);
  REPROTECT(out, out_prot_idx);

  int window_sizes[size_i];
  int window_starts[size_i];
  int window_stops[size_i];

  compute_window_sizes(window_sizes, out_indices, size_i);
  compute_window_starts(window_starts, window_sizes, size_i);
  compute_window_stops(window_stops, window_sizes, window_starts, size_i);

  int window_start = 0;
  int window_stop = size - 1;

  int window_start_index;
  int window_stop_index;

  SEXP last_start_position = PROTECT(Rf_ScalarInteger(1));
  SEXP last_stop_position = PROTECT(Rf_ScalarInteger(1));

  SEXP iteration = PROTECT(Rf_ScalarInteger(iteration_min));
  int* p_iteration_val = INTEGER(iteration);

  PROTECT_INDEX start_prot_idx;
  SEXP start = R_NilValue;
  PROTECT_WITH_INDEX(start, &start_prot_idx);

  PROTECT_INDEX stop_prox_idx;
  SEXP stop = R_NilValue;
  PROTECT_WITH_INDEX(stop, &stop_prox_idx);

  SEXP window = PROTECT(compact_seq(0, 0, true));
  int* p_window_val = INTEGER(window);

  // The result of each function call
  PROTECT_INDEX elt_prot_idx;
  SEXP elt = R_NilValue;
  PROTECT_WITH_INDEX(elt, &elt_prot_idx);

  SEXP container = PROTECT(make_slice_container(type));

  for (; *p_iteration_val <= iteration_max; ++(*p_iteration_val)) {
    if (*p_iteration_val % 1024 == 0) {
      R_CheckUserInterrupt();
    }

    if (!before_unbounded) {
      start = vec_slice_impl(starts, iteration);
      REPROTECT(start, start_prot_idx);

      window_start_index = locate_window_start_index(i, start, size_i, &last_start_position);
      window_start = window_starts[window_start_index];
    }

    if (!after_unbounded) {
      stop = vec_slice_impl(stops, iteration);
      REPROTECT(stop, stop_prox_idx);

      window_stop_index = locate_window_stop_index(i, stop, size_i, &last_stop_position);
      window_stop = window_stops[window_stop_index];
    }

    // This can happen with an irregular index, and is a sign of the full window
    // being between two index points and means we select nothing
    if (window_stop < window_start) {
      window_start = 0;
      window_stop = -1;
    }

    int window_size = window_stop - window_start + 1;
    init_compact_seq(p_window_val, window_start, window_size, true);

    slice_and_update_env(x, window, env, type, container);

    elt = Rf_eval(f_call, env);
    REPROTECT(elt, elt_prot_idx);

    SEXP out_index = VECTOR_ELT(out_indices, *p_iteration_val - 1);

    // TODO - Worry about needing fallback method when no proxy is defined / is a matrix
    // https://github.com/r-lib/vctrs/blob/8d12bfc0e29e056966e0549af619253253752a64/src/slice-assign.c#L46

    if (constrain) {
      elt = vctrs_cast(elt, ptype, strings_empty, strings_empty);
      REPROTECT(elt, elt_prot_idx);
      elt = vec_proxy(elt);
      REPROTECT(elt, elt_prot_idx);

      if (vec_size(elt) != 1) {
        stop_not_all_size_one(*p_iteration_val, vec_size(elt));
      }

      vec_assign_impl(out, out_index, elt, false);
    } else {
      int* p_out_index = INTEGER(out_index);
      int out_index_size = vec_size(out_index);

      for (int j = 0; j < out_index_size; ++j) {
        SET_VECTOR_ELT(out, p_out_index[j] - 1, elt);
      }
    }

  }

  out = vec_restore(out, ptype, r_int(size));
  REPROTECT(out, out_prot_idx);

  out = copy_names(out, x, type);
  REPROTECT(out, out_prot_idx);

  UNPROTECT(9);
  return out;
}

// -----------------------------------------------------------------------------

static int locate_window_start_index(SEXP i, SEXP start, int size, SEXP* p_last_start_position) {
  SEXP last_start_position = *p_last_start_position;
  int* p_last_start_position_val = INTEGER(last_start_position);

  PROTECT_INDEX i_position_prot_idx;
  SEXP i_position = vec_slice_impl(i, last_start_position);
  PROTECT_WITH_INDEX(i_position, &i_position_prot_idx);

  while(compare_lt(i_position, 0, start, 0)) {
    if (*p_last_start_position_val == size) {
      UNPROTECT(1);
      return(size - 1);
    }

    (*p_last_start_position_val)++;

    i_position = vec_slice_impl(i, last_start_position);
    REPROTECT(i_position, i_position_prot_idx);
  }

  UNPROTECT(1);
  return *p_last_start_position_val - 1;
}

static int locate_window_stop_index(SEXP i, SEXP stop, int size, SEXP* p_last_stop_position) {
  SEXP last_stop_position = *p_last_stop_position;
  int* p_last_stop_position_val = INTEGER(last_stop_position);

  PROTECT_INDEX i_position_prot_idx;
  SEXP i_position = vec_slice_impl(i, last_stop_position);
  PROTECT_WITH_INDEX(i_position, &i_position_prot_idx);

  while(compare_lte(i_position, 0, stop, 0)) {
    if (*p_last_stop_position_val == size) {
      UNPROTECT(1);
      return(size - 1);
    }

    (*p_last_stop_position_val)++;

    i_position = vec_slice_impl(i, last_stop_position);
    REPROTECT(i_position, i_position_prot_idx);
  }

  UNPROTECT(1);
  // - 1 - 1 (convert to C index + it always goes 1 too far)
  return *p_last_stop_position_val - 2;
}

// -----------------------------------------------------------------------------

static void stop_range_start_past_stop(SEXP starts, SEXP stops) {
  SEXP call = PROTECT(
    Rf_lang3(
      Rf_install("stop_range_start_past_stop"),
      starts,
      stops
    )
  );

  Rf_eval(call, slide_ns_env);
  Rf_error("Internal error: `stop_range_start_past_stop()` should have jumped earlier");
}

static void check_starts_not_past_stops(SEXP starts, SEXP stops) {
  bool any_gt = vec_any_gt(starts, stops);

  if (any_gt) {
    stop_range_start_past_stop(starts, stops);
  }
}

// -----------------------------------------------------------------------------

// map_int(x, vec_size)
static void compute_window_sizes(int* window_sizes, SEXP x, int n) {
  for (int i = 0; i < n; ++i) {
    window_sizes[i] = vec_size(VECTOR_ELT(x, i));
  }
}

static void compute_window_starts(int* window_starts, int* window_sizes, int n) {
  // First start is always 0
  window_starts[0] = 0;

  int sum = 0;

  // Then we do a cumsum() to get the rest of the starts
  for (int i = 1; i < n; ++i) {
    sum += window_sizes[i - 1];
    window_starts[i] = sum;
  }
}

static void compute_window_stops(int* window_stops, int* window_sizes, int* window_starts, int n) {
  for (int i = 0; i < n; ++i) {
    window_stops[i] = window_starts[i] + window_sizes[i] - 1;
  }
}

// -----------------------------------------------------------------------------

static int adjust_iteration_min(int iteration_min, SEXP range, SEXP i, int size) {
  SEXP first = PROTECT(Rf_ScalarInteger(1));
  SEXP i_first = PROTECT(vec_slice_impl(i, first));
  SEXP range_first = PROTECT(vec_slice_impl(range, first));

  if (compare_gt(i_first, 0, range_first, 0)) {
    int forward_adjustment = 0;

    for (int i = 0; i < size; ++i) {
      if (compare_gt(i_first, 0, range, i)) {
        forward_adjustment++;
      }
    }

    iteration_min = iteration_min + forward_adjustment;
  }

  UNPROTECT(3);
  return iteration_min;
}

static int adjust_iteration_max(int iteration_max, SEXP range, SEXP i, int size) {
  SEXP last = PROTECT(Rf_ScalarInteger(size));
  SEXP i_last = PROTECT(vec_slice_impl(i, last));
  SEXP range_last = PROTECT(vec_slice_impl(range, last));

  if (compare_lt(i_last, 0, range_last, 0)) {
    int backward_adjustment = 0;

    for (int i = 0; i < size; ++i) {
      if (compare_lt(i_last, 0, range, i)) {
        backward_adjustment++;
      }
    }

    iteration_max = iteration_max - backward_adjustment;
  }

  UNPROTECT(3);
  return iteration_max;
}