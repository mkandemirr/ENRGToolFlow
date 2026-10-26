#include <algorithm>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include <TAxis.h>
#include <TBox.h>
#include <TCanvas.h>
#include <TFile.h>
#include <TGraph.h>
#include <TLatex.h>
#include <TPad.h>
#include <TROOT.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TTree.h>

#include "ConfigReader.h"
#include "Event.h"
#include "INIReader.h"

// Returns 0 on success, non-zero on failure.
int drawWaveforms(const INIReader& iniReader);

int main(int argc, char* argv[])
{
   if (argc < 2)
   {
      std::cerr << "Usage: ./bin/drawWaveforms configs/drawWaveforms/config.ini"
                << std::endl;
      return 1;
   }

   INIReader iniReader(argv[1]);

   if (iniReader.ParseError() != 0)
   {
      std::cerr << "Error: could not load config file: "
                << argv[1] << std::endl;
      return 1;
   }

   return drawWaveforms(iniReader);
}

namespace
{
// Line colours of the channel rows, repeated if more than eight channels are drawn.
const int kChannelColors[8] = {kBlue + 1, kOrange + 7, kGreen + 2, kRed + 1,
                               kViolet + 1, kOrange + 3, kPink + 6, kGray + 2};

const double kPanelTopMargin = 0.03;

struct DisplaySettings
{
   UInt_t zoomStartNs;
   UInt_t zoomEndNs;
   UInt_t nEnvelopeBins;
   double minYHalfRangeMv;
   double yPaddingMv;
};

struct ChannelRange
{
   int minMv = 0;
   int maxMv = 0;
   UInt_t minIndex = 0;
};

// Minimum and maximum of the samples first..last (inclusive).
ChannelRange FindRange(const Waveform& waveform, UInt_t first, UInt_t last)
{
   ChannelRange range;
   range.minMv = waveform.GetValueMv(first);
   range.maxMv = range.minMv;
   range.minIndex = first;

   for (UInt_t i = first + 1; i <= last; ++i)
   {
      const int value = waveform.GetValueMv(i);

      if (value < range.minMv)
      {
         range.minMv = value;
         range.minIndex = i;
      }

      range.maxMv = std::max(range.maxMv, value);
   }

   return range;
}

// The y axis spans at least 0 +- minYHalfRangeMv, so that noise-only channels are
// not stretched to fill the panel.
double LowerYLimit(const ChannelRange& range, const DisplaySettings& settings)
{
   return std::min(-settings.minYHalfRangeMv, range.minMv - settings.yPaddingMv);
}

double UpperYLimit(const ChannelRange& range, const DisplaySettings& settings)
{
   return std::max(settings.minYHalfRangeMv, range.maxMv + settings.yPaddingMv);
}

void StyleGraph(TGraph* graph, int color, double yMin, double yMax)
{
   graph->SetLineColor(color);
   graph->SetLineWidth(1);
   graph->SetMinimum(yMin);
   graph->SetMaximum(yMax);
   graph->GetXaxis()->SetLabelSize(0.10);
   graph->GetYaxis()->SetLabelSize(0.10);
   graph->GetXaxis()->SetTitleSize(0.10);
   graph->GetYaxis()->SetTitleSize(0.11);
   graph->GetYaxis()->SetTitleOffset(0.32);
   graph->GetXaxis()->SetTitleOffset(0.95);
   graph->GetYaxis()->SetNdivisions(504);
   graph->GetXaxis()->SetNdivisions(510);
   graph->GetXaxis()->SetTickLength(0.06);
   graph->GetYaxis()->CenterTitle();
}

// Full-window graph as a min/max envelope: each bin contributes its minimum and
// maximum, so single-sample spikes stay visible with far fewer points.
TGraph* MakeEnvelopeGraph(const Waveform& waveform, UInt_t nBins)
{
   const UInt_t nSamples = waveform.GetNSamples();
   nBins = std::max<UInt_t>(1, std::min(nBins, nSamples));
   TGraph* graph = new TGraph(2 * nBins);

   for (UInt_t bin = 0; bin < nBins; ++bin)
   {
      const UInt_t first = static_cast<ULong64_t>(bin) * nSamples / nBins;
      const UInt_t end = static_cast<ULong64_t>(bin + 1) * nSamples / nBins;
      const auto [minIt, maxIt] = std::minmax_element(waveform.GetValuesMv() + first,
                                                      waveform.GetValuesMv() + end);
      const double xUs = 0.5 * (first + end) * 1e-3;
      graph->SetPoint(2 * bin, xUs, *minIt);
      graph->SetPoint(2 * bin + 1, xUs, *maxIt);
   }

   return graph;
}

std::string DescribeQualityFlags(UInt_t flags)
{
   if (flags == 0)
   {
      return "none";
   }

   std::string text;

   for (int bit = 0; bit < Event::kNQualityBits; ++bit)
   {
      if (flags & (1u << bit))
      {
         text += (text.empty() ? "" : ", ") + std::string(Event::GetQualityBitName(bit));
      }
   }

   return text;
}

// Draws one event on the canvas.
void DrawEventPage(TCanvas* canvas, const Event& event, Long64_t entry,
                   const std::string& fileLabel, const std::vector<int>& channelIds,
                   const DisplaySettings& settings)
{
   const size_t nRows = channelIds.size();
   std::vector<std::optional<Channel>> channels(nRows);
   UInt_t nSamplesShown = 0;

   for (size_t row = 0; row < nRows; ++row)
   {
      channels[row] = event.GetChannel(channelIds[row]);

      if (channels[row])
      {
         nSamplesShown = std::max(nSamplesShown,
                                  channels[row]->GetWaveform().GetNSamples());
      }
   }

   canvas->Clear();
   canvas->cd();

   TLatex latex;
   latex.SetNDC();
   latex.SetTextFont(62);
   latex.SetTextSize(0.0125);
   latex.DrawLatex(0.01, 0.982,
                   Form("%s   |   entry %lld   |   eventCounter %u   |   t_{0} = %.6f s"
                        "   |   qualityFlags %u (%s)",
                        fileLabel.c_str(), entry, event.GetEventCounter(),
                        event.GetTriggerTimeNs() * 1e-9, event.GetQualityFlags(),
                        DescribeQualityFlags(event.GetQualityFlags()).c_str()));
   latex.SetTextFont(42);
   latex.SetTextSize(0.0105);
   latex.DrawLatex(0.01, 0.968,
                   Form("%zu channels, up to %u samples (1 sample = 1 ns). Left: full "
                        "window [#mus], min/max envelope in %u bins. Right: samples "
                        "%u-%u (red band on the left).",
                        nRows, nSamplesShown, settings.nEnvelopeBins,
                        settings.zoomStartNs, settings.zoomEndNs));

   TPad* mainPad = new TPad("mainPad", "", 0.0, 0.0, 1.0, 0.962);
   mainPad->SetBit(kCanDelete);
   mainPad->Draw();
   mainPad->Divide(2, nRows, 0.002, 0.001);

   TBox zoomBand;
   zoomBand.SetFillColorAlpha(kRed, 0.25);
   zoomBand.SetLineWidth(0);

   for (size_t row = 0; row < nRows; ++row)
   {
      const int color = kChannelColors[row % 8];
      const bool lastRow = (row == nRows - 1);
      const double bottomMargin = lastRow ? 0.30 : 0.10;

      // Left: full window.
      mainPad->cd(2 * row + 1);
      gPad->SetMargin(0.075, 0.015, bottomMargin, kPanelTopMargin);

      if (!channels[row])
      {
         latex.SetTextSize(0.12);
         latex.SetTextColor(kRed + 1);
         latex.DrawLatex(0.1, 0.5, Form("channel %d is not in this event",
                                        channelIds[row]));
         latex.SetTextColor(kBlack);
         continue;
      }

      const Waveform& waveform = channels[row]->GetWaveform();
      const UInt_t lastSample = waveform.GetNSamples() - 1;
      const ChannelRange full = FindRange(waveform, 0, lastSample);
      const double yMin = LowerYLimit(full, settings);
      const double yMax = UpperYLimit(full, settings);

      TGraph* envelope = MakeEnvelopeGraph(waveform, settings.nEnvelopeBins);
      envelope->SetBit(kCanDelete);
      StyleGraph(envelope, color, yMin, yMax);
      envelope->GetXaxis()->SetLimits(0, waveform.GetNSamples() * 1e-3);
      envelope->GetYaxis()->SetTitle(Form("ch%d [mV]", channels[row]->GetChannelId()));

      if (lastRow)
      {
         envelope->GetXaxis()->SetTitle("time [#mus] = sample index / 1000");
      }

      // The zoom window as a shaded band; the envelope is drawn again on top of it.
      envelope->Draw("AL");
      zoomBand.DrawBox(settings.zoomStartNs * 1e-3, yMin,
                       settings.zoomEndNs * 1e-3, yMax);
      envelope->Draw("L");

      latex.SetTextSize(0.08);
      latex.SetTextAlign(31);
      latex.DrawLatex(0.975, lastRow ? 0.34 : 0.14,
                      Form("min %d mV @ %u ns | max %d mV",
                           full.minMv, full.minIndex, full.maxMv));
      latex.SetTextAlign(11);

      // Right: zoom window, every sample.
      mainPad->cd(2 * row + 2);
      gPad->SetMargin(0.10, 0.02, bottomMargin, kPanelTopMargin);

      if (settings.zoomStartNs > lastSample)
      {
         latex.SetTextSize(0.12);
         latex.SetTextColor(kRed + 1);
         latex.DrawLatex(0.1, 0.5, Form("zoom window outside the record (%u samples)",
                                        waveform.GetNSamples()));
         latex.SetTextColor(kBlack);
         continue;
      }

      const UInt_t zoomLast = std::min(settings.zoomEndNs, lastSample);
      const UInt_t nZoom = zoomLast - settings.zoomStartNs + 1;
      const ChannelRange zoomRange = FindRange(waveform, settings.zoomStartNs, zoomLast);
      TGraph* zoom = new TGraph(nZoom);
      zoom->SetBit(kCanDelete);

      for (UInt_t i = 0; i < nZoom; ++i)
      {
         const UInt_t sample = settings.zoomStartNs + i;
         zoom->SetPoint(i, sample, waveform.GetValueMv(sample));
      }

      StyleGraph(zoom, color, LowerYLimit(zoomRange, settings),
                 UpperYLimit(zoomRange, settings));
      zoom->GetXaxis()->SetLimits(settings.zoomStartNs, zoomLast);
      zoom->GetXaxis()->SetNdivisions(505);

      if (lastRow)
      {
         zoom->GetXaxis()->SetTitle("sample index (1 sample = 1 ns)");
      }

      zoom->Draw("AL");
   }
}
}

int drawWaveforms(const INIReader& iniReader)
{
   // Read every key before stopping, so that all config problems are reported.
   ConfigReader config(iniReader);
   const std::string inputFileName = config.GetString("input", "rootFile");
   const std::string inputTreeName = config.GetString("input", "treeName");
   const std::string pdfFileName = config.GetString("output", "pdfFile");
   const bool overwriteOutput = config.GetBoolean("output", "overwriteOutput");
   const long long firstEntry = config.GetInteger("analysis", "firstEntry");
   const long long maxEvents = config.GetInteger("analysis", "maxEvents");
   const std::vector<int> channelIds = config.GetIntegerList("analysis", "channels");
   const long long zoomStartNs = config.GetInteger("analysis", "zoomStartNs");
   const long long zoomEndNs = config.GetInteger("analysis", "zoomEndNs");

   DisplaySettings settings;
   settings.nEnvelopeBins = config.GetInteger("analysis", "nEnvelopeBins");
   settings.minYHalfRangeMv = config.GetReal("analysis", "minYHalfRangeMv");
   settings.yPaddingMv = config.GetReal("analysis", "yPaddingMv");

   if (!config.IsValid())
   {
      return 1;
   }

   // Checked before the conversion to UInt_t: a negative value would wrap to a huge
   // sample index.
   if (zoomStartNs < 0 || zoomEndNs < zoomStartNs)
   {
      std::cerr << "Error: zoom window must satisfy 0 <= zoomStartNs <= zoomEndNs"
                << std::endl;
      return 1;
   }

   settings.zoomStartNs = zoomStartNs;
   settings.zoomEndNs = zoomEndNs;

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

   const Long64_t nEntries = tree->GetEntries();

   if (firstEntry < 0 || firstEntry >= nEntries)
   {
      std::cerr << "Error: firstEntry " << firstEntry << " is outside the tree (0-"
                << nEntries - 1 << ")" << std::endl;
      return 1;
   }

   const Long64_t lastEntry = (maxEvents < 0)
      ? nEntries - 1 : std::min<Long64_t>(nEntries - 1, firstEntry + maxEvents - 1);

   Event event;
   event.SetBranchAddresses(tree);

   gROOT->SetBatch(kTRUE);
   gStyle->SetOptStat(0);
   gStyle->SetOptTitle(0);

   const std::string fileLabel = gSystem->BaseName(inputFileName.c_str());
   TCanvas canvas("canvas", "", 1600, 150 + 250 * channelIds.size());
   canvas.Print((pdfFileName + "[").c_str());

   Long64_t nPages = 0;
   const Long64_t nToDraw = lastEntry - firstEntry + 1;

   for (Long64_t entry = firstEntry; entry <= lastEntry; ++entry)
   {
      tree->GetEntry(entry);
      DrawEventPage(&canvas, event, entry, fileLabel, channelIds, settings);
      canvas.Print(pdfFileName.c_str());
      ++nPages;

      if (nToDraw >= 10 && nPages % (nToDraw / 10) == 0)
      {
         std::cout << "Progress: " << 100 * nPages / nToDraw << "% (" << nPages
                   << " events)" << std::endl;
      }
   }

   canvas.Print((pdfFileName + "]").c_str());

   std::cout << "\n=== drawWaveforms summary ===" << std::endl;
   std::cout << "Input file:      " << inputFileName << std::endl;
   std::cout << "Output file:     " << pdfFileName << std::endl;
   std::cout << "Events drawn:    " << nPages << " (entries " << firstEntry << "-"
             << lastEntry << ")" << std::endl;
   std::cout << "Events skipped:  0" << std::endl;

   return 0;
}
