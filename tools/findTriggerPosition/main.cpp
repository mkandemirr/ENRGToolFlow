#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <TCanvas.h>
#include <TFile.h>
#include <TH1D.h>
#include <TLatex.h>
#include <TLine.h>
#include <TPad.h>
#include <TROOT.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TTree.h>

#include "ConfigReader.h"
#include "Event.h"
#include "INIReader.h"

// Returns 0 on success, non-zero on failure.
int findTriggerPosition(const INIReader& iniReader);

int main(int argc, char* argv[])
{
   if (argc < 2)
   {
      std::cerr << "Usage: ./bin/findTriggerPosition "
                << "configs/findTriggerPosition/config.ini" << std::endl;
      return 1;
   }

   INIReader iniReader(argv[1]);

   if (iniReader.ParseError() != 0)
   {
      std::cerr << "Error: could not load config file: "
                << argv[1] << std::endl;
      return 1;
   }

   return findTriggerPosition(iniReader);
}

namespace
{
// Sums over all events of one channel, indexed by sample number.
struct ChannelAccumulator
{
   std::vector<Long64_t> sumMv;       // sum of value_mv
   std::vector<Long64_t> nEntries;    // events whose record contains this sample
   std::vector<Long64_t> occupancy;   // events whose minimum sample is here
   std::map<UInt_t, Long64_t> recordLengthCounts;
   Long64_t nEvents = 0;
};

struct ChannelResult
{
   int channelId = 0;
   Long64_t nEvents = 0;
   bool hasMinimum = false;   // average waveform has a negative minimum
   double minimumNs = 0;      // T_min: position of the average-waveform minimum
   double minimumMv = 0;
   bool hasLeadingEdge = false;
   double leadingEdgeNs = 0;  // T_cf: cfFraction crossing before the minimum
   double occupancyPeakNs = 0;  // T_occ: most frequent position of the event minimum
   double occupancyFwhmNs = 0;
   std::unique_ptr<TH1D> average;
   std::unique_ptr<TH1D> occupancy;
};

// Adds one channel record: every sample to the sums, the position of its minimum
// (first one if several samples share it) to the occupancy.
void Accumulate(ChannelAccumulator& accumulator, const Waveform& waveform)
{
   const UInt_t nSamples = waveform.GetNSamples();

   if (nSamples == 0)
   {
      return;
   }

   if (nSamples > accumulator.sumMv.size())
   {
      accumulator.sumMv.resize(nSamples, 0);
      accumulator.nEntries.resize(nSamples, 0);
      accumulator.occupancy.resize(nSamples, 0);
   }

   const Short_t* values = waveform.GetValuesMv();
   UInt_t minimumIndex = 0;

   for (UInt_t i = 0; i < nSamples; ++i)
   {
      accumulator.sumMv[i] += values[i];
      ++accumulator.nEntries[i];

      if (values[i] < values[minimumIndex])
      {
         minimumIndex = i;
      }
   }

   ++accumulator.occupancy[minimumIndex];
   ++accumulator.recordLengthCounts[nSamples];
   ++accumulator.nEvents;
}

// Histogram with one bin per sample, centred on the sample index.
std::unique_ptr<TH1D> MakeSampleHistogram(const std::string& name,
                                          const std::string& title, size_t nSamples)
{
   auto histogram = std::make_unique<TH1D>(name.c_str(), title.c_str(), nSamples,
                                           -0.5, nSamples - 0.5);
   histogram->SetDirectory(nullptr);
   return histogram;
}

// Full width at half maximum of the peak at peakIndex, with linear interpolation
// between the samples on each side.
double ComputeFwhm(const std::vector<Long64_t>& counts, size_t peakIndex)
{
   const double half = 0.5 * counts[peakIndex];
   size_t left = peakIndex;

   while (left > 0 && counts[left - 1] > half)
   {
      --left;
   }

   double leftNs = left;

   if (left > 0)
   {
      leftNs = (left - 1) + (half - counts[left - 1])
                            / static_cast<double>(counts[left] - counts[left - 1]);
   }

   size_t right = peakIndex;

   while (right + 1 < counts.size() && counts[right + 1] > half)
   {
      ++right;
   }

   double rightNs = right;

   if (right + 1 < counts.size())
   {
      rightNs = right + (counts[right] - half)
                        / static_cast<double>(counts[right] - counts[right + 1]);
   }

   return rightNs - leftNs;
}

ChannelResult Analyse(int channelId, const ChannelAccumulator& accumulator,
                      double cfFraction)
{
   ChannelResult result;
   result.channelId = channelId;
   result.nEvents = accumulator.nEvents;

   const size_t nSamples = accumulator.sumMv.size();

   if (nSamples == 0)
   {
      return result;
   }

   result.average = MakeSampleHistogram(
      Form("average_ch%d", channelId),
      Form("ch%d average waveform;sample index (1 sample = 1 ns);mean value [mV]",
           channelId), nSamples);
   result.occupancy = MakeSampleHistogram(
      Form("occupancy_ch%d", channelId),
      Form("ch%d position of the event minimum;sample index (1 sample = 1 ns);events",
           channelId), nSamples);

   std::vector<double> average(nSamples, 0);
   size_t minimumIndex = 0;
   size_t peakIndex = 0;

   for (size_t i = 0; i < nSamples; ++i)
   {
      average[i] = static_cast<double>(accumulator.sumMv[i]) / accumulator.nEntries[i];
      result.average->SetBinContent(i + 1, average[i]);
      result.occupancy->SetBinContent(i + 1, accumulator.occupancy[i]);

      if (average[i] < average[minimumIndex])
      {
         minimumIndex = i;
      }

      if (accumulator.occupancy[i] > accumulator.occupancy[peakIndex])
      {
         peakIndex = i;
      }
   }

   result.minimumNs = minimumIndex;
   result.minimumMv = average[minimumIndex];
   result.hasMinimum = (result.minimumMv < 0);
   result.occupancyPeakNs = peakIndex;
   result.occupancyFwhmNs = ComputeFwhm(accumulator.occupancy, peakIndex);

   if (!result.hasMinimum)
   {
      return result;
   }

   // Walk back from the minimum to the first sample above the cfFraction level.
   const double level = cfFraction * result.minimumMv;

   for (size_t j = minimumIndex; j > 0; --j)
   {
      if (average[j - 1] > level)
      {
         result.leadingEdgeNs = (j - 1) + (level - average[j - 1])
                                          / (average[j] - average[j - 1]);
         result.hasLeadingEdge = true;
         break;
      }
   }

   return result;
}

double Median(std::vector<double> values)
{
   std::sort(values.begin(), values.end());
   const size_t n = values.size();
   return (n % 2 == 1) ? values[n / 2] : 0.5 * (values[n / 2 - 1] + values[n / 2]);
}

// Vertical line over the full height of the current pad.
void DrawVerticalLine(double x, int color, int style)
{
   gPad->Update();
   double yMin = gPad->GetUymin();
   double yMax = gPad->GetUymax();

   if (gPad->GetLogy())
   {
      yMin = std::pow(10, yMin);
      yMax = std::pow(10, yMax);
   }

   TLine line;
   line.SetLineColor(color);
   line.SetLineStyle(style);
   line.SetLineWidth(2);
   line.DrawLine(x, yMin, x, yMax);
}

void StyleHistogram(TH1D* histogram, int color)
{
   histogram->SetLineColor(color);
   histogram->SetStats(false);
   histogram->GetXaxis()->SetTitleSize(0.045);
   histogram->GetYaxis()->SetTitleSize(0.045);
   histogram->GetXaxis()->SetLabelSize(0.04);
   histogram->GetYaxis()->SetLabelSize(0.04);
   histogram->GetYaxis()->SetTitleOffset(1.1);
}

// Copy of a histogram drawn on the current pad and owned by it.
TH1D* DrawCopy(const TH1D& histogram, int color, double xMin, double xMax)
{
   TH1D* copy = static_cast<TH1D*>(histogram.Clone());
   copy->SetDirectory(nullptr);
   copy->SetBit(kCanDelete);
   StyleHistogram(copy, color);

   if (xMin < xMax)
   {
      copy->GetXaxis()->SetRangeUser(xMin, xMax);
   }

   copy->Draw("HIST");
   return copy;
}

struct PageSettings
{
   double plotHalfWindowNs;
   double cfFraction;
   double daqBaselineEndNs;
   bool hasExpected;
   double expectedNs;
};

void DrawChannelPage(TCanvas* canvas, const ChannelResult& result,
                     const std::string& fileLabel, const PageSettings& settings)
{
   canvas->Clear();
   canvas->cd();

   TLatex latex;
   latex.SetNDC();
   latex.SetTextFont(62);
   latex.SetTextSize(0.022);
   latex.DrawLatex(0.01, 0.965, Form("%s   |   channel %d   |   %lld events",
                                     fileLabel.c_str(), result.channelId,
                                     result.nEvents));

   if (!result.average)
   {
      latex.SetTextColor(kRed + 1);
      latex.DrawLatex(0.1, 0.5, Form("channel %d is in no event", result.channelId));
      return;
   }

   latex.SetTextFont(42);
   latex.SetTextSize(0.018);
   latex.DrawLatex(0.01, 0.94,
                   Form("Red: leading edge T_{cf} (%.2f of the minimum).  Blue dashed: "
                        "minimum T_{min}.  Magenta: occupancy peak T_{occ}.",
                        settings.cfFraction));
   latex.DrawLatex(0.01, 0.925, "Green dotted: nominal position.  Grey dashed: end of "
                                "the ENRGDAQ baseline window.");

   TPad* mainPad = new TPad("mainPad", "", 0.0, 0.0, 1.0, 0.91);
   mainPad->SetBit(kCanDelete);
   mainPad->Draw();
   mainPad->Divide(2, 2, 0.003, 0.003);

   const double centreNs = result.hasLeadingEdge ? result.leadingEdgeNs
                                                 : result.occupancyPeakNs;
   const double zoomMin = centreNs - settings.plotHalfWindowNs;
   const double zoomMax = centreNs + settings.plotHalfWindowNs;
   const double fullMax = result.average->GetXaxis()->GetXmax();

   // The minimum line is only drawn when the average waveform has a dip.
   auto drawAverageLines = [&](bool full)
   {
      if (result.hasLeadingEdge)
      {
         DrawVerticalLine(result.leadingEdgeNs, kRed + 1, 1);
      }

      if (result.hasMinimum)
      {
         DrawVerticalLine(result.minimumNs, kBlue + 1, 2);
      }

      if (settings.hasExpected)
      {
         DrawVerticalLine(settings.expectedNs, kGreen + 2, 3);
      }

      if (full)
      {
         DrawVerticalLine(settings.daqBaselineEndNs, kGray + 2, 2);
      }
   };

   // Top left: average waveform, full record.
   mainPad->cd(1);
   gPad->SetMargin(0.12, 0.03, 0.12, 0.08);
   DrawCopy(*result.average, kBlack, 0, 0);
   drawAverageLines(true);

   // Top right: occupancy, full record, log scale.
   mainPad->cd(2);
   gPad->SetMargin(0.12, 0.03, 0.12, 0.08);
   gPad->SetLogy();
   TH1D* occupancyFull = DrawCopy(*result.occupancy, kMagenta + 2, 0, 0);
   occupancyFull->SetMinimum(0.5);
   DrawVerticalLine(settings.daqBaselineEndNs, kGray + 2, 2);

   // Bottom left: average waveform around the measured position.
   mainPad->cd(3);
   gPad->SetMargin(0.12, 0.03, 0.12, 0.08);
   DrawCopy(*result.average, kBlack, zoomMin, zoomMax);
   drawAverageLines(false);

   if (result.hasMinimum)
   {
      TLine level;
      level.SetLineColor(kRed + 1);
      level.SetLineStyle(3);
      level.DrawLine(std::max(zoomMin, -0.5), settings.cfFraction * result.minimumMv,
                     std::min(zoomMax, fullMax), settings.cfFraction * result.minimumMv);
   }

   latex.SetTextSize(0.045);
   latex.SetTextAlign(31);
   latex.DrawLatex(0.96, 0.18,
                   result.hasLeadingEdge
                      ? Form("T_{cf} = %.1f ns,  T_{min} = %.0f ns (%.1f mV)",
                             result.leadingEdgeNs, result.minimumNs, result.minimumMv)
                      : "no leading edge found");

   // Bottom right: occupancy around the measured position.
   mainPad->cd(4);
   gPad->SetMargin(0.12, 0.03, 0.12, 0.08);
   DrawCopy(*result.occupancy, kMagenta + 2, zoomMin, zoomMax);
   DrawVerticalLine(result.occupancyPeakNs, kMagenta + 2, 1);
   latex.DrawLatex(0.96, 0.85, Form("T_{occ} = %.0f ns,  FWHM = %.1f ns",
                                    result.occupancyPeakNs, result.occupancyFwhmNs));
   latex.SetTextAlign(11);
}

void DrawSummaryPage(TCanvas* canvas, const std::vector<std::string>& lines)
{
   canvas->Clear();
   canvas->cd();

   TLatex latex;
   latex.SetNDC();
   latex.SetTextFont(82);
   latex.SetTextSize(0.02);

   for (size_t i = 0; i < lines.size(); ++i)
   {
      latex.DrawLatex(0.03, 0.95 - 0.03 * i, lines[i].c_str());
   }
}
}

int findTriggerPosition(const INIReader& iniReader)
{
   // Read every key before stopping, so that all config problems are reported.
   ConfigReader config(iniReader);
   const std::string inputFileName = config.GetString("input", "rootFile");
   const std::string inputTreeName = config.GetString("input", "treeName");
   const std::string pdfFileName = config.GetString("output", "pdfFile");
   const bool overwriteOutput = config.GetBoolean("output", "overwriteOutput");
   const long long maxEvents = config.GetInteger("analysis", "maxEvents");
   const std::vector<int> channelIds = config.GetIntegerList("analysis", "channels");
   const double postTriggerPercent = config.GetReal("analysis", "postTriggerPercent");
   const double cfFraction = config.GetReal("analysis", "cfFraction");
   const double daqBaselineFraction = config.GetReal("analysis", "daqBaselineFraction");
   const double plotHalfWindowNs = config.GetReal("analysis", "plotHalfWindowNs");

   if (!config.IsValid())
   {
      return 1;
   }

   bool validValues = true;

   for (int channel : channelIds)
   {
      if (channel < 0 || channel > 255)
      {
         std::cerr << "Error: invalid channel " << channel << " in channels" << std::endl;
         validValues = false;
      }
   }

   if (postTriggerPercent != -1 && (postTriggerPercent < 0 || postTriggerPercent > 100))
   {
      std::cerr << "Error: postTriggerPercent must be -1 or between 0 and 100"
                << std::endl;
      validValues = false;
   }

   if (cfFraction <= 0 || cfFraction >= 1)
   {
      std::cerr << "Error: cfFraction must be between 0 and 1" << std::endl;
      validValues = false;
   }

   if (daqBaselineFraction < 0 || daqBaselineFraction > 1)
   {
      std::cerr << "Error: daqBaselineFraction must be between 0 and 1" << std::endl;
      validValues = false;
   }

   if (plotHalfWindowNs <= 0)
   {
      std::cerr << "Error: plotHalfWindowNs must be positive" << std::endl;
      validValues = false;
   }

   if (!validValues)
   {
      return 1;
   }

   namespace fs = std::filesystem;
   std::error_code errorCode;

   if (fs::exists(pdfFileName) && !overwriteOutput)
   {
      std::cerr << "Error: output file already exists (set overwriteOutput = true "
                << "to replace it): " << pdfFileName << std::endl;
      return 1;
   }

   const fs::path outputDirectory = fs::path(pdfFileName).parent_path();

   if (!outputDirectory.empty() && !fs::exists(outputDirectory))
   {
      fs::create_directories(outputDirectory, errorCode);

      if (errorCode)
      {
         std::cerr << "Error: could not create output directory "
                   << outputDirectory << ": " << errorCode.message() << std::endl;
         return 1;
      }
   }

   TFile inputFile(inputFileName.c_str(), "READ");

   if (inputFile.IsZombie())
   {
      std::cerr << "Error: could not open input file: " << inputFileName << std::endl;
      return 1;
   }

   TTree* tree = dynamic_cast<TTree*>(inputFile.Get(inputTreeName.c_str()));

   if (tree == nullptr)
   {
      std::cerr << "Error: TTree '" << inputTreeName << "' not found in "
                << inputFileName << std::endl;
      return 1;
   }

   Event event;
   event.SetBranchAddresses(tree);
   tree->SetBranchStatus("*", 0);
   tree->SetBranchStatus("channelId", 1);
   tree->SetBranchStatus("channelNSamples", 1);
   tree->SetBranchStatus("sampleValueMv", 1);

   const Long64_t nEntries = tree->GetEntries();
   const Long64_t nToRead = (maxEvents < 0) ? nEntries
                                            : std::min<Long64_t>(nEntries, maxEvents);
   std::vector<ChannelAccumulator> accumulators(channelIds.size());
   int nextProgressPercent = 10;

   for (Long64_t entry = 0; entry < nToRead; ++entry)
   {
      tree->GetEntry(entry);

      for (size_t c = 0; c < channelIds.size(); ++c)
      {
         if (auto channel = event.GetChannel(channelIds[c]))
         {
            Accumulate(accumulators[c], channel->GetWaveform());
         }
      }

      while (100 * (entry + 1) >= nextProgressPercent * nToRead
             && nextProgressPercent <= 100)
      {
         std::cout << "Progress: " << nextProgressPercent << "% (" << entry + 1
                   << " events)" << std::endl;
         nextProgressPercent += 10;
      }
   }

   // Record length of the run: the most frequent length over all analysed channels.
   std::map<UInt_t, Long64_t> recordLengthCounts;

   for (const ChannelAccumulator& accumulator : accumulators)
   {
      for (const auto& [length, count] : accumulator.recordLengthCounts)
      {
         recordLengthCounts[length] += count;
      }
   }

   if (recordLengthCounts.empty())
   {
      std::cerr << "Error: none of the channels is in the events read" << std::endl;
      return 1;
   }

   UInt_t recordLength = 0;
   Long64_t recordLengthCount = 0;

   for (const auto& [length, count] : recordLengthCounts)
   {
      if (count > recordLengthCount)
      {
         recordLength = length;
         recordLengthCount = count;
      }
   }

   std::vector<ChannelResult> results;
   std::vector<double> leadingEdges;

   for (size_t c = 0; c < channelIds.size(); ++c)
   {
      results.push_back(Analyse(channelIds[c], accumulators[c], cfFraction));

      if (results.back().hasLeadingEdge)
      {
         leadingEdges.push_back(results.back().leadingEdgeNs);
      }
   }

   PageSettings settings;
   settings.plotHalfWindowNs = plotHalfWindowNs;
   settings.cfFraction = cfFraction;
   settings.daqBaselineEndNs = daqBaselineFraction * recordLength;
   settings.hasExpected = (postTriggerPercent >= 0);
   settings.expectedNs = recordLength * (1 - postTriggerPercent / 100);

   const bool hasRunPosition = !leadingEdges.empty();
   const double runPositionNs = hasRunPosition ? Median(leadingEdges) : 0;

   // The summary goes to the terminal and to the first page of the PDF.
   std::vector<std::string> lines;
   lines.push_back(Form("%-4s %9s %10s %9s %10s %10s %9s %8s", "ch", "nEvents",
                        "T_min[ns]", "min[mV]", "T_cf[ns]", "T_occ[ns]", "FWHM[ns]",
                        "dT[ns]"));

   for (const ChannelResult& result : results)
   {
      if (result.nEvents == 0)
      {
         lines.push_back(Form("%-4d %9d   channel not found", result.channelId, 0));
         continue;
      }

      const std::string leadingEdge = result.hasLeadingEdge
         ? Form("%10.1f", result.leadingEdgeNs) : Form("%10s", "-");
      const std::string offset = result.hasLeadingEdge
         ? Form("%8.1f", result.leadingEdgeNs - runPositionNs) : Form("%8s", "-");
      lines.push_back(Form("%-4d %9lld %10.0f %9.2f %s %10.0f %9.1f %s",
                           result.channelId, result.nEvents, result.minimumNs,
                           result.minimumMv, leadingEdge.c_str(),
                           result.occupancyPeakNs, result.occupancyFwhmNs,
                           offset.c_str()));
   }

   lines.push_back("");
   lines.push_back(Form("Record length (most frequent): %u samples", recordLength));

   if (recordLengthCounts.size() > 1)
   {
      lines.push_back(Form("  (%zu different record lengths found)",
                           recordLengthCounts.size()));
   }

   lines.push_back(hasRunPosition
                   ? Form("Trigger position (median T_cf of %zu channels): %.1f ns",
                          leadingEdges.size(), runPositionNs)
                   : std::string("Trigger position: no channel has a leading edge"));

   if (settings.hasExpected)
   {
      lines.push_back(Form("Nominal position (%.1f %% post-trigger): %.1f ns",
                           postTriggerPercent, settings.expectedNs));

      if (hasRunPosition)
      {
         lines.push_back(Form("Measured - nominal: %.1f ns",
                              runPositionNs - settings.expectedNs));
      }
   }
   else
   {
      lines.push_back("Nominal position: postTriggerPercent = -1, comparison skipped");
   }

   lines.push_back(Form("ENRGDAQ baseline window: samples 0-%.0f (%.0f %% of the record)",
                        settings.daqBaselineEndNs, 100 * daqBaselineFraction));

   std::vector<std::string> warnings;

   for (const ChannelResult& result : results)
   {
      if (result.hasLeadingEdge && result.leadingEdgeNs < settings.daqBaselineEndNs)
      {
         warnings.push_back(Form("Warning: ch%d leading edge %.1f ns is inside the "
                                 "ENRGDAQ baseline window", result.channelId,
                                 result.leadingEdgeNs));
      }
      else if (result.nEvents > 0 && !result.hasLeadingEdge)
      {
         warnings.push_back(Form("Warning: ch%d has no leading edge in its average "
                                 "waveform", result.channelId));
      }
   }

   gROOT->SetBatch(kTRUE);
   gStyle->SetOptStat(0);
   gStyle->SetOptTitle(0);

   const std::string fileLabel = gSystem->BaseName(inputFileName.c_str());
   // Not wider than tall: ROOT rotates landscape PDF pages.
   TCanvas canvas("canvas", "", 1600, 1600);
   canvas.Print((pdfFileName + "[").c_str());

   std::vector<std::string> pageLines = {fileLabel + ": trigger position", ""};
   pageLines.insert(pageLines.end(), lines.begin(), lines.end());
   pageLines.insert(pageLines.end(), warnings.begin(), warnings.end());
   DrawSummaryPage(&canvas, pageLines);
   canvas.Print(pdfFileName.c_str());

   for (const ChannelResult& result : results)
   {
      DrawChannelPage(&canvas, result, fileLabel, settings);
      canvas.Print(pdfFileName.c_str());
   }

   canvas.Print((pdfFileName + "]").c_str());

   std::cout << "\n=== findTriggerPosition summary ===" << std::endl;
   std::cout << "Input file:       " << inputFileName << std::endl;
   std::cout << "Output file:      " << pdfFileName << std::endl;
   std::cout << "Events processed: " << nToRead << " of " << nEntries << std::endl;
   std::cout << "Events skipped:   0\n" << std::endl;

   for (const std::string& line : lines)
   {
      std::cout << line << std::endl;
   }

   for (const std::string& warning : warnings)
   {
      std::cerr << warning << std::endl;
   }

   return 0;
}
