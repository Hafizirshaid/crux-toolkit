#include "ParamMedicApplication.h"
#include "io/carp.h"
#include "io/SpectrumCollectionFactory.h"
#include "parameter.h"
#include "util/Params.h"

#include <cmath>
#include <numeric>

using namespace Crux;
using namespace std;

const double SQRT_2_PI = 2.50662827463;
// maximum proportion of precursor delta-masses that can be 0, otherwise we give up
const double MAX_PROPORTION_PRECURSOR_DELTAS_ZERO = 0.5;
// maximum peaks to use to fit a mixed distribution
const int MAX_PEAKPAIRS = 100000;
// multipliers to transform standard error values into algorithm parameters
const double PRECURSOR_SIGMA_MULTIPLIER = 11.130897;
const double FRAGMENT_SIGMA_MULTIPLIER = 4.763766;
// separation between averagine peaks used for binning spectra
const double AVERAGINE_PEAK_SEPARATION = 1.000495;
// minimum allowed values for sigma of the estimated normal
const double MIN_SIGMA_PPM = 0.01;
const double MIN_SIGMA_TH = 0.00001;

ParamMedicApplication::ParamMedicApplication() {
}

ParamMedicApplication::~ParamMedicApplication() {
}

int ParamMedicApplication::main(int argc, char** argv) {
  if (!numeric_limits<double>::is_iec559) {
    carp(CARP_FATAL, "Something went wrong.");
  }
  ParamMedicErrorCalculator errCalc;
  vector<string> files = Params::GetStrings("spectrum-file");
  for (vector<string>::const_iterator i = files.begin(); i != files.end(); i++) {
    carp(CARP_INFO, "Processing input file %s...", i->c_str());
    SpectrumCollection* collection = SpectrumCollectionFactory::create(*i);
    collection->parse();
    for (SpectrumIterator j = collection->begin(); j != collection->end(); j++) {
      errCalc.processSpectrum(*j);
    }
    delete collection;
    errCalc.clearBins();
  }

  // calculate mass error distributions
  double precursorSigmaPpm = 0;
  double fragmentSigmaPpm = 0;
  double fragmentSigmaTh = 0;
  double precursorPredictionPpm = 0;
  double fragmentPredictionPpm = 0;
  double fragmentPredictionTh = 0;
  errCalc.calcMassErrorDist(
    &precursorSigmaPpm,
    &fragmentSigmaPpm,
    &fragmentSigmaTh,
    &precursorPredictionPpm,
    &fragmentPredictionPpm,
    &fragmentPredictionTh
  );

  carp(CARP_DEBUG, "precursor ppm standard deviation: %f", precursorSigmaPpm);
  carp(CARP_DEBUG, "fragment standard deviation (Th): %f", fragmentSigmaTh);
  carp(CARP_DEBUG, "fragment standard deviation (ppm): %f", fragmentSigmaPpm);
  carp(CARP_INFO, "Precursor error estimate (ppm): %.2f", precursorPredictionPpm);
  carp(CARP_INFO, "Fragment bin size estimate (Th): %.4f", fragmentPredictionTh);
  carp(CARP_INFO, "Fragment bin size estimate (ppm): %.2f", fragmentPredictionPpm);
  return 0;
}

string ParamMedicApplication::getName() const {
  return "param-medic";
}

string ParamMedicApplication::getDescription() const {
  return "";
}

vector<string> ParamMedicApplication::getArgs() const {
  string arr[] = {
    "spectrum-file+"
  };
  return vector<string>(arr, arr + sizeof(arr) / sizeof(string));
}

vector<string> ParamMedicApplication::getOptions() const {
  string arr[] = {
    "verbosity",
    "spectrum-parser",
    "min-precursor-mz",
    "max-precursor-mz",
    "min-frag-mz",
    "max-frag-mz",
    "min-scan-frag-peaks",
    "max-precursor-delta-ppm",
    "charge",
    "top-n-frag-peaks",
    "pair-top-n-frag-peaks",
    "min-common-frag-peaks",
    "max-scan-separation",
    "min-peak-pairs"
  };
  return vector<string>(arr, arr + sizeof(arr) / sizeof(string));
}

vector< pair<string, string> > ParamMedicApplication::getOutputs() const {
  vector< pair<string, string> > outputs;
  outputs.push_back(make_pair("stdout",
    "the estimated parameter values for precursor mass tolerance (in ppm) and "
    "fragment bin size (in Th), as well as the standard deviations of the "
    "estimated error distributions for precursor and fragment masses."));
  return outputs;
}

bool ParamMedicApplication::needsOutputDirectory() const {
  return false;
}

ParamMedicErrorCalculator::ParamMedicErrorCalculator():
  numTotalSpectra_(0), numPassingSpectra_(0) {
  lowestPrecursorBinStartMz_ = Params::GetDouble("min-precursor-mz") -
    fmod(Params::GetDouble("min-precursor-mz"), AVERAGINE_PEAK_SEPARATION / Params::GetInt("charge"));
  lowestFragmentBinStartMz_ = Params::GetDouble("min-frag-mz") -
    fmod(Params::GetDouble("min-frag-mz"), AVERAGINE_PEAK_SEPARATION);
  numPrecursorBins_ = getBinIndexPrecursor(Params::GetDouble("max-precursor-mz")) + 1;
  numFragmentBins_ = getBinIndexFragment(Params::GetDouble("max-frag-mz")) + 1;
}

ParamMedicErrorCalculator::~ParamMedicErrorCalculator() {
  for (vector< pair<const Peak*, const Peak*> >::const_iterator i = pairedFragmentPeaks_.begin();
      i != pairedFragmentPeaks_.begin();
      i++) {
    delete i->first;
    delete i->second;
  }
}

void ParamMedicErrorCalculator::processSpectrum(Spectrum* spectrum) {
  ++numTotalSpectra_;

  if (spectrum->getNumPeaks() < Params::GetInt("min-scan-frag-peaks")) {
    return;
  }

  double precursorMz = getPrecursorMz(spectrum);
  if (!(Params::GetDouble("min-precursor-mz") <= precursorMz && precursorMz <= Params::GetDouble("max-precursor-mz"))) {
    return;
  }

  ++numPassingSpectra_;
  // pull out the top fragments by intensity
  spectrum->sortPeaks(_PEAK_INTENSITY);
  spectrum->truncatePeaks(Params::GetInt("top-n-frag-peaks"));

  int precursorBinIndex = getBinIndexPrecursor(precursorMz);
  map<int, Spectrum*>::const_iterator prevIter = spectra_.find(precursorBinIndex);
  if (prevIter != spectra_.end()) {
    // there was a previous spectrum in this bin; check to see if they're a pair
    const Spectrum* prev = prevIter->second;
    const double precursorMzPrev = getPrecursorMz(prev);
    const double precursorMzDiffPpm = (precursorMz - precursorMzPrev) * MILLION / precursorMz;
    // check precursor and scan count between the scans
    if (abs(precursorMzDiffPpm) <= Params::GetDouble("max-precursor-delta-ppm") &&
        abs(spectrum->getFirstScan() - prev->getFirstScan()) <= Params::GetInt("max-scan-separation")) {
      // count the fragment peaks in common
      vector< pair<const Peak*, const Peak*> > pairedFragments = pairFragments(prev, spectrum);
      if (pairedFragments.size() >= Params::GetInt("min-common-frag-peaks")) {
        // we've got a pair! record everything
        sort(pairedFragments.begin(), pairedFragments.end(), sortPairedFragments);
        vector< pair<const Peak*, const Peak*> >::const_iterator stop =
          pairedFragments.size() >= Params::GetInt("pair-top-n-frag-peaks")
            ? pairedFragments.begin() + Params::GetInt("pair-top-n-frag-peaks")
            : pairedFragments.end();
        for (vector< pair<const Peak*, const Peak*> >::const_iterator i = pairedFragments.begin();
            i != stop;
            i++) {
          pairedFragmentPeaks_.push_back(make_pair(
            new Peak(i->first->getIntensity(), i->first->getLocation()),
            new Peak(i->second->getIntensity(), i->second->getLocation())));
        }
        pairedPrecursorMzs_.push_back(make_pair(precursorMzPrev, precursorMz));
      }
    }
  }
  // make the new spectrum its bin's representative
  spectra_[precursorBinIndex] = spectrum;
}

void ParamMedicErrorCalculator::clearBins() {
  spectra_.clear();
}

void ParamMedicErrorCalculator::calcMassErrorDist(
  double* precursorSigmaPpm,
  double* fragmentSigmaPpm,
  double* fragmentSigmaTh,
  double* precursorPredictionPpm,
  double* fragmentPredictionPpm,
  double* fragmentPredictionTh
) {
  carp(CARP_INFO, "Processed %d total spectra", numTotalSpectra_);
  carp(CARP_INFO, "Processed %d qualifying spectra", numPassingSpectra_);
  carp(CARP_INFO, "Precursor pairs: %d", pairedPrecursorMzs_.size());
  carp(CARP_INFO, "Fragment pairs: %d", pairedFragmentPeaks_.size());

  if (pairedPrecursorMzs_.size() > MAX_PEAKPAIRS) {
    carp(CARP_INFO, "Using %d of %d peak pairs for precursor...",
         MAX_PEAKPAIRS, pairedPrecursorMzs_.size());
    random_shuffle(pairedPrecursorMzs_.begin(), pairedPrecursorMzs_.end(), myrandom_limit);
    pairedPrecursorMzs_.resize(MAX_PEAKPAIRS);
  }

  vector<double> precursorDistancesTh;
  vector<double> precursorDistancesPpm;
  int numZeroPrecursorDeltas = 0;
  for (vector< pair<double, double> >::const_iterator i = pairedPrecursorMzs_.begin();
       i != pairedPrecursorMzs_.end();
       i++) {
    double diffTh = i->first - i->second;
    if (diffTh == 0) {
      ++numZeroPrecursorDeltas;
    }
    precursorDistancesTh.push_back(diffTh);
    precursorDistancesPpm.push_back(diffTh * MILLION / i->first);
  }

  // check for conditions that would cause us to bomb out
  if (precursorDistancesTh.size() < Params::GetInt("min-peak-pairs")) {
    carp(CARP_FATAL, "Need >= %d peak pairs to fit mixed distribution. Got only %d",
         Params::GetInt("min-peak-pairs"), precursorDistancesTh.size());
  }
  double proportionPrecursorMzsZero = (double)numZeroPrecursorDeltas / pairedPrecursorMzs_.size();
  carp(CARP_DEBUG, "proportion zero: %f", proportionPrecursorMzsZero);
  if (proportionPrecursorMzsZero > MAX_PROPORTION_PRECURSOR_DELTAS_ZERO) {
    carp(CARP_FATAL, "Too high a proportion of precursor mass differences (%f) are exactly 0. "
                     "Some processing has been done on this run that param-medic can't handle. "
                     "You should investigate what that processing might be.",
         proportionPrecursorMzsZero);
  }

  if (pairedFragmentPeaks_.size() > MAX_PEAKPAIRS) {
    carp(CARP_INFO, "Using %d of %d peak pairs for fragment...",
         MAX_PEAKPAIRS, pairedFragmentPeaks_.size());
    random_shuffle(pairedFragmentPeaks_.begin(), pairedFragmentPeaks_.end(), myrandom_limit);
    pairedFragmentPeaks_.resize(MAX_PEAKPAIRS);
  }

  vector<double> fragmentDistancesTh;
  vector<double> fragmentDistancesPpm;
  for (vector< pair<const Peak*, const Peak*> >::const_iterator i = pairedFragmentPeaks_.begin();
       i != pairedFragmentPeaks_.end();
       i++) {
    double diffTh = i->first->getLocation() - i->second->getLocation();
    fragmentDistancesTh.push_back(diffTh);
    fragmentDistancesPpm.push_back(diffTh * MILLION / i->first->getLocation());
  }

  // estimate the parameters of the component distributions for each of the mixed distributions
  double precursorMuPpm2Measures, precursorSigmaPpm2Measures;
  estimateMuSigma(precursorDistancesPpm, MIN_SIGMA_PPM,
                  &precursorMuPpm2Measures, &precursorSigmaPpm2Measures);
  double fragmentMuPpm2Measures, fragmentSigmaPpm2Measures;
  estimateMuSigma(fragmentDistancesPpm, MIN_SIGMA_PPM,
                  &fragmentMuPpm2Measures, &fragmentSigmaPpm2Measures);
  double fragmentMuTh2Measures, fragmentSigmaTh2Measures;
  estimateMuSigma(fragmentDistancesTh, MIN_SIGMA_TH,
                  &fragmentMuTh2Measures, &fragmentSigmaTh2Measures);

  carp(CARP_DEBUG, "precursor_mu_ppm_2measures: %f", precursorMuPpm2Measures);
  carp(CARP_DEBUG, "precursor_sigma_ppm_2measures: %f", precursorSigmaPpm2Measures);
  carp(CARP_DEBUG, "fragment_mu_ppm_2measures: %f", fragmentMuPpm2Measures);
  carp(CARP_DEBUG, "fragment_sigma_ppm_2measures: %f", fragmentSigmaPpm2Measures);
  carp(CARP_DEBUG, "fragment_mu_th_2measures: %f", fragmentMuTh2Measures);
  carp(CARP_DEBUG, "fragment_sigma_th_2measures: %f", fragmentSigmaTh2Measures);

  // what we have now measured, in the fit Gaussians, is the sum of two errors.
  // "the sum of two independent normally distributed random variables is normal,
  // with its mean being the sum of the two means, and its variance being the sum of the two variances
  // (i.e., the square of the standard deviation is the sum of the squares of the standard deviations)."
  // https://en.wikipedia.org/wiki/Sum_of_normally_distributed_random_variables
  // Practically speaking, this distinction doesn't matter one bit. I'm just doing it for interpretability
  // of the sigma value
  *precursorSigmaPpm = sqrt(pow(precursorSigmaPpm2Measures, 2));
  *fragmentSigmaPpm = sqrt(pow(fragmentSigmaPpm2Measures, 2));
  *fragmentSigmaTh = sqrt(pow(fragmentSigmaTh2Measures, 2));

  // generate predictions by multiplying by empirically-derived values
  *precursorPredictionPpm = PRECURSOR_SIGMA_MULTIPLIER * *precursorSigmaPpm;
  *fragmentPredictionTh = FRAGMENT_SIGMA_MULTIPLIER * *fragmentSigmaTh;
  *fragmentPredictionPpm = FRAGMENT_SIGMA_MULTIPLIER * *fragmentSigmaPpm;
}

void ParamMedicErrorCalculator::estimateMuSigma(
  const vector<double>& data,
  double minSigma,
  double* muFit,
  double* sigmaFit
) {
  double dataMin = data[0];
  double dataMax = dataMin;
  double muMixedDist = dataMin;
  for (vector<double>::const_iterator i = data.begin() + 1; i != data.end(); i++) {
    muMixedDist += *i;
    if (*i < dataMin) {
      dataMin = *i;
    }
    if (*i > dataMax) {
      dataMax = *i;
    }
  }
  muMixedDist /= data.size();

  double sigmaMixedDist = 0;
  for (vector<double>::const_iterator i = data.begin(); i != data.end(); i++) {
    sigmaMixedDist += pow(*i - muMixedDist, 2);
  }
  sigmaMixedDist = sqrt(sigmaMixedDist / data.size());

  carp(CARP_DEBUG, "mixed distribution: min %f, max %f, mean %f, sd %f",
       dataMin, dataMax, muMixedDist, sigmaMixedDist);

  // model the observed distribution as a mixture of Gaussian and uniform
  ParamMedicModel model(muMixedDist, sigmaMixedDist, minSigma, dataMin, dataMax);
  // fit the mixture model with EM
  double improvement = model.fit(data);
  carp(CARP_DEBUG, "model improvement: %f", improvement);

  *muFit = model.getMu();
  *sigmaFit = model.getSigma();
  carp(CARP_DEBUG, "fit: mean=%f, sigma=%f", *muFit, *sigmaFit);
}

int ParamMedicErrorCalculator::getBinIndexPrecursor(double mz) const {
  return (int)((mz - lowestPrecursorBinStartMz_) / (AVERAGINE_PEAK_SEPARATION / Params::GetInt("charge")));
}

int ParamMedicErrorCalculator::getBinIndexFragment(double mz) const {
  return (int)((mz - lowestFragmentBinStartMz_) / AVERAGINE_PEAK_SEPARATION);
}

double ParamMedicErrorCalculator::getPrecursorMz(const Spectrum* spectrum) const {
  const vector<SpectrumZState>& zStates = spectrum->getZStates();
  for (vector<SpectrumZState>::const_iterator i = zStates.begin(); i != zStates.end(); i++) {
    if (i->getCharge() == Params::GetInt("charge")) {
      return i->getMZ();
    }
  }
  return -1;
}

vector< pair<const Peak*, const Peak*> > ParamMedicErrorCalculator::pairFragments(
  const Spectrum* prev,
  const Spectrum* cur
) const {
  map<int, const Peak*> mapPrev = binFragments(prev);
  map<int, const Peak*> mapCur = binFragments(cur);
  vector< pair<const Peak*, const Peak*> > pairs;
  for (map<int, const Peak*>::const_iterator i = mapPrev.begin(); i != mapPrev.end(); i++) {
    map<int, const Peak*>::const_iterator j = mapCur.find(i->first);
    if (j != mapCur.end()) {
      pairs.push_back(make_pair(i->second, j->second));
    }
  }
  return pairs;
}

map<int, const Peak*> ParamMedicErrorCalculator::binFragments(const Spectrum* spectrum) const {
  map<int, const Peak*> binFragmentMap;
  set<int> binsToRemove;
  for (PeakIterator i = spectrum->begin(); i != spectrum->end(); i++) {
    FLOAT_T mz = (*i)->getLocation();
    FLOAT_T intensity = (*i)->getIntensity();
    if (mz < Params::GetDouble("min-frag-mz")) {
      continue;
    }
    int binIndex = getBinIndexFragment(mz);
    if (binFragmentMap.find(binIndex) != binFragmentMap.end()) {
      binsToRemove.insert(binIndex);
    } else {
      binFragmentMap[binIndex] = *i;
    }
  }
  for (set<int>::const_iterator i = binsToRemove.begin(); i != binsToRemove.end(); i++) {
    binFragmentMap.erase(*i);
  }
  return binFragmentMap;
}

bool ParamMedicErrorCalculator::sortPairedFragments(
  const pair<const Peak*, const Peak*> x,
  const pair<const Peak*, const Peak*> y
) {
  return min(x.first->getIntensity(), x.second->getIntensity()) <
         min(y.first->getIntensity(), y.second->getIntensity());
}

ParamMedicModel::ParamMedicModel(double nMean, double nStd, double nMinStd, double uStart, double uEnd):
  normal_(NormalDistribution(nMean, nStd, nMinStd)), uniform_(UniformDistribution(uStart, uEnd)) {
  weights_[0] = weights_[1] = log(0.5);
  summaries_[0] = summaries_[1] = 0;
}

ParamMedicModel::~ParamMedicModel() {
}

double ParamMedicModel::fit(const vector<double>& data) {
  const double stopThreshold = 0.1;
  const int maxIterations = 1e8;

  double initialLogProbSum = -numeric_limits<double>::infinity();
  double lastLogProbSum;
  int i = 0;
  double improvement = numeric_limits<double>::infinity();
  while (improvement > 0.1 && i < maxIterations + 1) {
    fromSummaries();
    double logProbSum = summarize(data);

    if (i == 0) {
      initialLogProbSum = logProbSum;
    } else {
      improvement = logProbSum - lastLogProbSum;
      carp(CARP_DETAILED_DEBUG, "Improvement: %f", improvement);
    }
    ++i;
    lastLogProbSum = logProbSum;
  }
  clearSummaries();
  carp(CARP_DEBUG, "Total Improvement: %f", lastLogProbSum - initialLogProbSum);
  return lastLogProbSum - initialLogProbSum;
}

double ParamMedicModel::getMu() const {
  return normal_.getMu();
}

double ParamMedicModel::getSigma() const {
  return normal_.getSigma();
}

double ParamMedicModel::summarize(const vector<double>& x) {
  vector<double> r;
  r.reserve(x.size() * 2);
  normal_.logProbability(x, &r);
  uniform_.logProbability(x, &r);

  double logProbSum = 0;
  for (size_t i = 0; i < x.size(); i++) {
    double total = -numeric_limits<double>::infinity();

    for (size_t j = 0; j < 2; j++) {
      r[j * x.size() + i] += weights_[j];
      total = pairLse(total, r[j * x.size() + i]);
    }

    for (size_t j = 0; j < 2; j++) {
      r[j * x.size() + i] = exp(r[j * x.size() + i]);
      summaries_[j] += r[j * x.size() + i];
    }

    logProbSum += total;
  }

  normal_.summarize(x, &r[0]);
  uniform_.summarize(x, &r[r.size() / 2]);
  return logProbSum;
}

void ParamMedicModel::fromSummaries() {
  double sum = summaries_[0] + summaries_[1];
  if (sum == 0) {
    return;
  }

  summaries_[0] /= sum;
  summaries_[1] /= sum;

  normal_.fromSummaries();
  weights_[0] = log(summaries_[0]);
  summaries_[0] = 0;

  uniform_.fromSummaries();
  weights_[1] = log(summaries_[1]);
  summaries_[1] = 0;
}

void ParamMedicModel::clearSummaries() {
  summaries_[0] = summaries_[1] = 0;
  normal_.clearSummaries();
  uniform_.clearSummaries();
}

double ParamMedicModel::pairLse(double x, double y) {
  const double inf = numeric_limits<double>::infinity();
  if (x == inf || y == inf) {
    return inf;
  } else if (x == -inf) {
    return y;
  } else if (y == -inf) {
    return x;
  } else if (x > y) {
    return x + log(exp(y - x) + 1);
  }
  return y + log(exp(x - y) + 1);
}

ParamMedicModel::NormalDistribution::NormalDistribution(double mean, double std, double minStd):
  mu_(mean), sigma_(std), minStd_(minStd),
  logSigmaSqrt2Pi_(-log(std * SQRT_2_PI)), twoSigmaSquared_(2 * pow(std, 2)) {
  clearSummaries();
}

ParamMedicModel::NormalDistribution::~NormalDistribution() {
}

double ParamMedicModel::NormalDistribution::getMu() const {
  return mu_;
}

double ParamMedicModel::NormalDistribution::getSigma() const {
  return sigma_;
}

void ParamMedicModel::NormalDistribution::logProbability(const vector<double>& x, vector<double>* r) const {
  for (vector<double>::const_iterator i = x.begin(); i != x.end(); i++) {
    r->push_back(logSigmaSqrt2Pi_ - pow(*i - mu_, 2) / twoSigmaSquared_);
  }
}

void ParamMedicModel::NormalDistribution::summarize(const vector<double>& x, double* weights) {
  for (size_t i = 0; i < x.size(); i++) {
    summaries_[0] += weights[i];
    summaries_[1] += weights[i] * x[i];
    summaries_[2] += weights[i] * pow(x[i], 2);
  }
}

void ParamMedicModel::NormalDistribution::fromSummaries() {
  if (summaries_[0] == 0) {
    return;
  }
  mu_ = summaries_[1] / summaries_[0];
  double sigma = sqrt(
    summaries_[2] / summaries_[0] - pow(summaries_[1], 2) / pow(summaries_[0], 2));
  if (sigma < minStd_) {
    sigma = minStd_;
  }
  sigma_ = sigma;
  clearSummaries();
}

void ParamMedicModel::NormalDistribution::clearSummaries() {
  summaries_[0] = summaries_[1] = summaries_[2] = 0;
}

ParamMedicModel::UniformDistribution::UniformDistribution(double start, double end):
  start_(start), end_(end), logP_(-log(end - start)) {
  clearSummaries();
}

ParamMedicModel::UniformDistribution::~UniformDistribution() {
}

void ParamMedicModel::UniformDistribution::logProbability(const vector<double>& x, vector<double>* r) const {
  for (vector<double>::const_iterator i = x.begin(); i != x.end(); i++) {
    r->push_back(start_ <= *i && *i <= end_ ? logP_ : -numeric_limits<double>::infinity());
  }
}

void ParamMedicModel::UniformDistribution::summarize(const vector<double>& x, double* weights) {
  for (size_t i = 0; i < x.size(); i++) {
    if (weights[i] <= 0) {
      continue;
    }
    double value = x[i];
    if (value < summaries_[0]) {
      summaries_[0] = value;
    }
    if (value > summaries_[1]) {
      summaries_[1] = value;
    }
  }
}

void ParamMedicModel::UniformDistribution::fromSummaries() {
  start_ = summaries_[0];
  end_ = summaries_[1];
  logP_ = -log(end_ - start_);
  clearSummaries();
}

void ParamMedicModel::UniformDistribution::clearSummaries() {
  summaries_[0] = numeric_limits<double>::infinity();
  summaries_[1] = -numeric_limits<double>::infinity();
}

