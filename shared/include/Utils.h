#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>

#include <Rtypes.h>

class TTree;
class Waveform;

// Stateless helper functions shared by the Tools (CODING_STANDARDS.md §3).
namespace Utils
{
   struct BaselineResult
   {
      double meanMv = 0;
      double rmsMv = 0;
      UInt_t nSamples = 0;   // samples actually used
   };

   // Mean and rms of the samples [firstSample, firstSample + nSamples) of a
   // waveform. The window is cut at the end of the waveform; if no sample is
   // left, the result has nSamples = 0 and mean and rms are 0.
   BaselineResult ComputeBaseline(const Waveform& waveform, UInt_t firstSample,
                                  UInt_t nSamples);

   // Joins friend trees to a tree by entry number (CODING_STANDARDS.md §5.1).
   //
   // friendFiles[i] holds a tree named friendTreeNames[i]; it is added with that
   // name as alias, so its branches can also be reached as "<name>.<branch>".
   // Before anything is added, every friend is checked: it must have the same
   // number of entries as the tree and the same eventCounter in every entry. On
   // the first problem an error is printed and false is returned; no friend is
   // added then.
   //
   // Branch addresses the caller has already set on the tree are kept.
   bool AddFriends(TTree* tree, const std::vector<std::string>& friendFiles,
                   const std::vector<std::string>& friendTreeNames);
}

#endif
