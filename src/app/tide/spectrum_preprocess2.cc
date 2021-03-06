// Benjamin Diament
//
// Mostly an implementation of ObservedPeakSet::PreprocessSpectrum(). See .h
// file.
// 
// Portions of this code are taken verbatim from Crux, specifically
// NormalizeEachRegion(). The region-defining code in PreprocessSpectrum() is
// also from Crux and inherits the attendant arcana. Some of this code is
// certainly at odds with the published intention of XCORR, but in order to
// preserve the results from Crux exactly we retain this legacy for now.
// TODO 251: revisit this.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <algorithm>
#include <math.h>
#include <gflags/gflags.h>
#include "spectrum_collection.h"
#include "spectrum_preprocess.h"
#include "mass_constants.h"
#include "max_mz.h"
#include "util/mass.h"
#include "util/Params.h"

using namespace std;

#ifdef DEBUG
DEFINE_int32(debug_spectrum_id, -1, "Spectrum number to debug");
DEFINE_int32(debug_charge, 0, "Charge to debug. 0 for all");
#endif

// This computes that part of the XCORR function where an average value of the
// peaks within a window surrounding each peak is subtracted from that peak.
// This version is a linear-time implementation of the subtraction. Linearity is
// accomplished by computing an array of partial sums.
static void SubtractBackground(double* observed, int end) {
  // operation is as follows: new_observed = observed -
  // average_within_window but average is computed as if the array
  // extended infinitely: denominator is same throughout array, even
  // near edges (where fewer elements have been summed)
  static const double multiplier = 1.0 / (MAX_XCORR_OFFSET * 2);

  double total = 0;
  vector<double> partial_sums(end+1);
  for (int i = 0; i < end; ++i)
    partial_sums[i] = (total += observed[i]);
  partial_sums[end] = total;

  for (int i = 0; i < end; ++i) {
    int right_index = min(end, i + MAX_XCORR_OFFSET);
    int left_index = max(0, i - MAX_XCORR_OFFSET - 1);
    observed[i] -= multiplier * (partial_sums[right_index] - partial_sums[left_index] - observed[i]);
  }
}

void ObservedPeakSet::PreprocessSpectrum(const Spectrum& spectrum, int charge,
                                         long int* num_range_skipped,
                                         long int* num_precursors_skipped,
                                         long int* num_isotopes_skipped,
                                         long int* num_retained) {
#ifdef DEBUG
  bool debug = (FLAGS_debug_spectrum_id == spectrum.SpectrumNumber()
                && (FLAGS_debug_charge == 0 || FLAGS_debug_charge == charge));
  if (debug)
    debug = true; // allows a breakpoint
#endif
  double precursor_mz = spectrum.PrecursorMZ();
  double experimental_mass_cut_off = (precursor_mz-MASS_PROTON)*charge+MASS_PROTON + 50;
  double max_peak_mz = spectrum.M_Z(spectrum.Size()-1);

  assert(MaxBin::Global().MaxBinEnd() > 0);

  max_mz_.InitBin(min(experimental_mass_cut_off, max_peak_mz));
  cache_end_ = MaxBin::Global().CacheBinEnd() * NUM_PEAK_TYPES;

  memset(peaks_, 0, sizeof(double) * MaxBin::Global().BackgroundBinEnd());

  if (Params::GetBool("skip-preprocessing")) {
    for (int i = 0; i < spectrum.Size(); ++i) {
      double peak_location = spectrum.M_Z(i);
      if (peak_location >= experimental_mass_cut_off) {
        continue;
      }
      int mz = MassConstants::mass2bin(peak_location);
      double intensity = spectrum.Intensity(i);
      if (intensity > peaks_[mz]) {
        peaks_[mz] = intensity;
      }
    }
  } else {
    bool remove_precursor = Params::GetBool("remove-precursor-peak");
    double precursor_tolerance = Params::GetDouble("remove-precursor-tolerance");
    double deisotope_threshold = Params::GetDouble("deisotope");
    int max_charge = spectrum.MaxCharge();

    // Fill peaks
    int largest_mz = 0;
    double highest_intensity = 0;
    for (int i = spectrum.Size() - 1; i >= 0; --i) {
      double peak_location = spectrum.M_Z(i);

      // Get rid of peaks beyond the possible range, given charge and precursor.
      if (peak_location >= experimental_mass_cut_off) {
        (*num_range_skipped)++;
        continue;
      }

      // Remove precursor peaks.
      if (remove_precursor &&
          fabs(peak_location - precursor_mz) <= precursor_tolerance ) {
        (*num_precursors_skipped)++;
        continue;
      }

      // Do Morpheus-style simple(-istic?) deisotoping.  "For each
      // peak, lower m/z peaks are considered. If the reference peak
      // lies where an expected peak would lie for a charge state from
      // one to the charge state of the precursor, within mass
      // tolerance, and is of lower abundance, the reference peak is
      // considered to be an isotopic peak and removed."
      double intensity = spectrum.Intensity(i);

      bool skip_peak = false;
      if (deisotope_threshold != 0.0) {
        for (int fragCharge = 1; fragCharge < max_charge; ++fragCharge) {
          double isotopic_peak = peak_location - (ISOTOPE_SPACING / fragCharge);
          double ppm_difference = ( peak_location * deisotope_threshold ) / 1e6;
          double isotopic_intensity = spectrum.MaxPeakInRange(isotopic_peak - ppm_difference,
                                                              isotopic_peak + ppm_difference);
        
          if (intensity < isotopic_intensity) {
            carp(CARP_DETAILED_DEBUG,
                 "Removing isotopic peak (%g, %g) because of peak in [%g, %g] with intensity %g.",
                 peak_location, intensity, isotopic_peak - ppm_difference,
                 isotopic_peak + ppm_difference, isotopic_intensity);
            skip_peak = true;
            break;
          }
        }
      }

      if (skip_peak) {
        (*num_isotopes_skipped)++;
        continue;
      } else {
        (*num_retained)++;
      }

      int mz = MassConstants::mass2bin(peak_location);
      if ((mz > largest_mz) && (intensity > 0)) {
        largest_mz = mz;
      }
      
      intensity = sqrt(intensity);
      if (intensity > highest_intensity) {
        highest_intensity = intensity;
      }
      if (intensity > peaks_[mz]) {
        peaks_[mz] = intensity;
      }
    }

    double intensity_cutoff = highest_intensity * 0.05;

    double normalizer = 0.0;
    int region_size = largest_mz / NUM_SPECTRUM_REGIONS + 1;
    for (int i = 0; i < NUM_SPECTRUM_REGIONS; ++i) {
      highest_intensity = 0;
      int high_index = i;
      for (int j = 0; j < region_size; ++j) {
        int index = i * region_size + j;
        if (peaks_[index] <= intensity_cutoff) {
          peaks_[index] = 0;
        }
        if (peaks_[index] > highest_intensity) {
          highest_intensity = peaks_[index];
          high_index = index;
        }
      }
      if (highest_intensity == 0) {
        continue;
      }
      normalizer = 50.0 / highest_intensity;
      for (int j = 0; j < region_size; ++j) {
        int index = i * region_size + j;
        if (peaks_[index] != 0) {
          peaks_[index] *= normalizer;
        }
      }
    }

#ifdef DEBUG
    if (debug) {
      cout << "GLOBAL MAX MZ: " << MaxMZ::Global().MaxBin() << ", " << MaxMZ::Global().BackgroundBinEnd()
           << ", " << MaxMZ::Global().CacheBinEnd() << endl;
      cout << "MAX MZ: " << max_mz_.MaxBin() << ", " << max_mz_.BackgroundBinEnd()
           << ", " << max_mz_.CacheBinEnd() << endl;
      ShowPeaks();
      cout << "====== SUBTRACTING BACKGROUND ======" << endl;
    }
#endif
  }
  SubtractBackground(peaks_, max_mz_.BackgroundBinEnd());

#ifdef DEBUG
  if (debug)
    ShowPeaks();
#endif
  MakeInteger();
  ComputeCache();
#ifdef DEBUG
  if (debug)
    ShowCache();
#endif
}

inline int round_to_int(double x) {
  if (x >= 0)
    return int(x + 0.5);
  return int(x - 0.5);
}

void ObservedPeakSet::MakeInteger() {
  // essentially cheap fixed-point arithmetic for peak intensities
  for(int i = 0; i < max_mz_.BackgroundBinEnd(); i++)
    Peak(PeakMain, i) = round_to_int(peaks_[i]*50000);
}

// See .h file. Computes and stores all transformations of the observed peak
// set.
void ObservedPeakSet::ComputeCache() {
  for (int i = 0; i < max_mz_.BackgroundBinEnd(); ++i) {
    // Instead of computing 10 * x, 25 * x, and 50 * x, we compute 2 *
    // x, 5 * x and 10 * x. This results in dot products that are 5
    // times too small, but the adjustments can be made at the last
    // moment e.g. when results are displayed. These smaller
    // multiplications allow us to use addition operations instead of
    // multiplications.
    int x = Peak(PeakMain, i);
    int y = x+x;
    Peak(LossPeak, i) = y;
    int z = y+y+x;
    Peak(FlankingPeak, i) = z;
    Peak(PrimaryPeak, i) = z+z;
  }

  for (int i = max_mz_.BackgroundBinEnd() * NUM_PEAK_TYPES; i < cache_end_; ++i) {
    cache_[i] = 0;
  }

  for (int i = 0; i < max_mz_.CacheBinEnd(); ++i) {
    int flanks = Peak(PrimaryPeak, i);
    if ( FP_ == true) {
        if (i > 0) {
          flanks += Peak(FlankingPeak, i-1);
        }
        if (i < max_mz_.CacheBinEnd() - 1) {
          flanks += Peak(FlankingPeak, i+1);
        }
    }
    int Y1 = flanks;
    if ( NL_ == true) {
        if (i > MassConstants::BIN_NH3) {
          Y1 += Peak(LossPeak, i-MassConstants::BIN_NH3);
        }
        if (i > MassConstants::BIN_H2O) {
          Y1 += Peak(LossPeak, i-MassConstants::BIN_H2O);
        }
    }
    Peak(PeakCombinedY1, i) = Y1;
    int B1 = Y1;
    Peak(PeakCombinedB1, i) = B1;
    Peak(PeakCombinedY2, i) = flanks;
    Peak(PeakCombinedB2, i) = flanks;
  }
}

// This dot product is replaced by calls to on-the-fly compiled code.
int ObservedPeakSet::DotProd(const TheoreticalPeakArr& theoretical) {
  int total = 0;
  TheoreticalPeakArr::const_iterator i = theoretical.begin();
  for (; i != theoretical.end(); ++i) {
    //if (i->Code() >= cache_end_)
    //  break;
    total += cache_[i->Code()];
  }
  return total;
}

#if 0
int ObservedPeakSet::DebugDotProd(const TheoreticalPeakArr& theoretical) {
  cout << "cache_end_=" << cache_end_ << endl;
  int total = 0;
  TheoreticalPeakArr::const_iterator i = theoretical.begin();
  for (; i != theoretical.end(); ++i) {
    cout << "DotProd Lookup(" << i->Bin() << "," << i->Type() << "):";
    if (i->Code() >= cache_end_) {
      cout << "code=" << i->Code() << "past cache_end_=" << cache_end_ << "; ignoring" << endl;
      continue;
    }
    total += cache_[i->Code()];
    cout << cache_[i->Code()] << "; total=" << total << endl;
  }
  return total;
}
#endif
