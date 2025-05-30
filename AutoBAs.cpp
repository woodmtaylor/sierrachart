#include "sierrachart.h"
#include <vector>
#include <algorithm> // For std::max/std::min, std::sort
#include <unordered_map>
#include <set>
#include <map>
#include <cmath>     // For std::fabs, std::sqrt, std::pow
#include <cfloat>    // For FLT_MAX, FLT_MIN
#include <string>    // For std::to_string, std::string
#include <functional> // For std::reference_wrapper
#include <initializer_list> // For std::max/min with {}
#include <numeric>   // For std::accumulate

// --- Undefine potential conflicting macros ---
#undef max
#undef min
// -----------------------------------------

SCDLLName("AUTO BAs")

// --- Data Structures ---
struct s_PriceLevelVolume { 
    float Price = 0.0f;
    float TotalVolume = 0.0f; 
    int NumberOfTrades = 0; 
};

using PriceVolumeMap = std::unordered_map<float, s_PriceLevelVolume>;

struct s_SessionProfile { 
    SCDateTime StartDateTime; 
    SCDateTime EndDateTime; 
    int BeginIndex; 
    int EndIndex; 
    float POC = 0.0f; 
    float ValueAreaHigh = 0.0f;
    float ValueAreaLow = 0.0f; 
    float TotalVolume = 0.0f; 
    float HighestPrice = -FLT_MAX; 
    float LowestPrice = FLT_MAX; 
    PriceVolumeMap PriceMap;
    int ChronologicalIndex = -1; 
    
    float GetRange() const { 
        if (HighestPrice <= -FLT_MAX || LowestPrice >= FLT_MAX || HighestPrice < LowestPrice) return 0.0f;
        return HighestPrice - LowestPrice; 
    } 
};

// Enhanced Balance Area struct with activation tracking
struct s_BalanceArea {
    int StartProfileChronoIndex = -1;
    int EndProfileChronoIndex = -1;
    SCDateTime StartDateTime;
    SCDateTime EndDateTime;
    int StartBarIndex = -1;
    int EndBarIndex = -1;
    float POC = 0.0f;
    float ValueAreaHigh = 0.0f;
    float ValueAreaLow = 0.0f;
    float HighestPrice = -FLT_MAX;
    float LowestPrice = FLT_MAX;
    float TotalVolume = 0.0f;
    std::vector<int> IncludedProfileIndices;
    std::string InitiationReason = "N/A";
    
    // NEW: Activation tracking
    bool IsActivated = false;
    SCDateTime ActivationDateTime;
    int ActivationBarIndex = -1;
    float ActivationPrice = 0.0f;
    std::string ActivationType = ""; // "Break_High", "Break_Low"
    bool ActivatedHigh = false;     // True if activated by breaking VA High
    bool ActivatedLow = false;      // True if activated by breaking VA Low
    
    // NEW: Extension tracking for intersections
    bool IsExtending = false;       // True when using extending rectangle
    int ExtensionEndIndex = -1;     // Where extension ends (due to intersection or chart end)
    std::string ExtensionEndReason = ""; // "Chart_End", "BA_Intersection", "Manual_Cutoff"
	bool WasCut = false;            // True when extension was cut by intersection
    
    float GetRange() const {
        if (HighestPrice <= -FLT_MAX || LowestPrice >= FLT_MAX || HighestPrice < LowestPrice) return 0.0f;
        return HighestPrice - LowestPrice;
    }
};

struct s_ProbeLineDrawingInfo { 
    int StartBarIndex = -1; 
    int EndBarIndexOfProfile = -1;
    float Price = 0.0f; 
    bool IsHighProbe = false; 
};

// PBAL struct
struct s_PBALDrawingInfo {
    int StartBarIndex = -1;
    int EndBarIndex = -1;           // Chart end or cut point
    float Price = 0.0f;
    bool IsHigh = false;            // true for PBAH, false for PBAL
    SCString OriginLabel;           // Original orange BA label
    std::string EndReason = "Chart_End"; // "Chart_End" or "Intersection"
    int OriginStartProfileIndex = -1;
    int OriginEndProfileIndex = -1;
    bool WasCut = false;
};

// Composite BA Struct
struct s_CompositeBalanceArea {
    int FirstBAIndex = -1;
    int SecondBAIndex = -1; 
    int ThirdBAIndex = -1;
    SCDateTime StartDateTime; 
    SCDateTime EndDateTime; 
    int StartBarIndex = -1; 
    int EndBarIndex = -1;
    float HighestPrice = -FLT_MAX; 
    float LowestPrice = FLT_MAX;
    std::string QualificationReason = "N/A";
    
    float GetRange() const { 
        if (HighestPrice <= -FLT_MAX || LowestPrice >= FLT_MAX || HighestPrice < LowestPrice) return 0.0f;
        return HighestPrice - LowestPrice; 
    }
};

// NEW: Struct for Distribution Statistics
struct s_DistributionStats {
    float mean = 0.0f;
    float stdDev = 0.0f;
    float skewness = 0.0f;
    float excessKurtosis = 0.0f; // Kurtosis - 3
    bool sufficientData = false;
    int numPriceLevelsWithVolume = 0;
};

// Enhanced Persistent Data Struct
struct s_BAStudyPersistentData {
    std::vector<s_BalanceArea> FinalizedBalanceAreas;
    std::vector<s_ProbeLineDrawingInfo> ProbeLinesToDraw;
    std::vector<s_PBALDrawingInfo> PBALsToDraw;
    std::vector<s_CompositeBalanceArea> CompositeBAs;
    
    // NEW: Activation tracking
    std::vector<s_BalanceArea> ActiveBalanceAreas;    // BAs that have been activated
    std::vector<int> CreatedActiveBADrawings;         // Track active BA extending rectangles
    
    int LastProfileCount = 0; 
    int LastNumberOfSessions = 0; 
    int LastReferenceStudyID = 0;
    float LastVAPercentage = 0.0f;
    float LastMinVolOverlap = 0.0f; 
    float LastMinVAOverlap = 0.0f;
    float LastRangeSimilarityPercent = 0.0f; 
    float LastHighLowTolerancePercent = 0.0f; 
    float LastRangeContPercent = 0.0f;
    bool LastDrawProbeLines = false; 
    COLORREF LastHighProbeColor = 0; 
    COLORREF LastLowProbeColor = 0;
    int LastProbeLineWidth = 0; 
    SubgraphLineStyles LastProbeLineStyle = LINESTYLE_SOLID;
    bool LastExtendProbeLines = false;
    bool LastDebugCompositeBA = false;
    bool LastDebugBAFormation = false;
    bool LastDrawCompositeRect = false;
    
    // NEW: Persistent data for normality filter inputs
    bool LastFilterByNormality = false;
    float LastMaxAbsSkewness = 0.0f;
    float LastMinExcessKurtosis = 0.0f;
    float LastMaxExcessKurtosis = 0.0f;
    
    // NEW: Track user-drawn status for manual adjustment capability
    bool LastAllowUserAdjustment = false;
	bool LastDrawActiveBAs = false;
    
    // Track all drawings created by this study for proper cleanup
    std::vector<int> CreatedBADrawings;        // Track all BA drawing line numbers
    std::vector<int> CreatedProbeDrawings;     // Track all probe drawing line numbers  
    std::vector<int> CreatedCompositeDrawings; // Track all composite drawing line numbers
    std::vector<int> CreatedLabelDrawings;     // Track all label drawing line numbers
    
    // Track which drawings have been manually adjusted by users
    std::set<int> UserAdjustedDrawings;        // Track any drawings that users have modified

};

// --- Calculation Functions ---

// NEW: Function to Calculate Volume Distribution Statistics
s_DistributionStats CalculateVolumeDistributionStats(const PriceVolumeMap& priceMap, float tickSize) {
   s_DistributionStats stats;
   std::vector<std::pair<float, float>> pvm_vec; // price, volume

   if (priceMap.empty()) {
       stats.sufficientData = false;
       return stats;
   }

   for (const auto& pair : priceMap) {
       if (pair.second.TotalVolume > 0.00001f) { // Only consider levels with some volume
           pvm_vec.push_back({pair.first, pair.second.TotalVolume});
       }
   }
   stats.numPriceLevelsWithVolume = static_cast<int>(pvm_vec.size());

   if (stats.numPriceLevelsWithVolume < 3) { // Need at least 3 distinct price levels for meaningful skew/kurtosis
       stats.sufficientData = false;
       // Calculate mean/stdDev if possible for basic info, even if not "sufficient" for higher moments
       if (stats.numPriceLevelsWithVolume > 0) {
           double tempTotalVolume = 0.0; // Use double for intermediate sums
           double sumPriceTimesVolume = 0.0;
           for (const auto& pv : pvm_vec) {
               tempTotalVolume += pv.second;
               sumPriceTimesVolume += static_cast<double>(pv.first) * pv.second;
           }
           if (tempTotalVolume > 0.00001f) {
               stats.mean = static_cast<float>(sumPriceTimesVolume / tempTotalVolume);
               if (stats.numPriceLevelsWithVolume > 1) {
                   double sumSquaredDevTimesVolume = 0.0;
                   for (const auto& pv : pvm_vec) {
                       double diff = static_cast<double>(pv.first) - stats.mean;
                       sumSquaredDevTimesVolume += pv.second * diff * diff;
                   }
                   stats.stdDev = std::sqrt(static_cast<float>(sumSquaredDevTimesVolume / tempTotalVolume));
               } else { // Single price level
                    stats.stdDev = 0.0f;
               }
           }
       }
       return stats; // Not enough data for skew/kurtosis
   }

   double currentTotalVolume = 0.0;
   double sumPriceTimesVolume = 0.0;
   for (const auto& pv : pvm_vec) {
       currentTotalVolume += pv.second;
       sumPriceTimesVolume += static_cast<double>(pv.first) * pv.second;
   }

   if (currentTotalVolume <= 0.00001f) { // Should be caught by pvm_vec.size check essentially
       stats.sufficientData = false;
       return stats;
   }
   stats.mean = static_cast<float>(sumPriceTimesVolume / currentTotalVolume);

   double sumSquaredDevTimesVolume = 0.0;
   for (const auto& pv : pvm_vec) {
       double diff = static_cast<double>(pv.first) - stats.mean;
       sumSquaredDevTimesVolume += pv.second * diff * diff;
   }
   stats.stdDev = std::sqrt(static_cast<float>(sumSquaredDevTimesVolume / currentTotalVolume));

   // If standard deviation is very small, higher moments are numerically unstable or profile is too spike-like.
   // Treat as a very peaked distribution (high kurtosis) and zero skewness.
   if (stats.stdDev < (tickSize / 100.0f)) { // Adjusted threshold: e.g. 1% of a tick
       stats.skewness = 0.0f;
       stats.excessKurtosis = 10.0f; // Arbitrary high value for extreme peakedness
       stats.sufficientData = true; // It's "sufficient" in the sense that it's a spike, higher moments are just defined this way.
       return stats;
   }

   double sumCubedStdDevTimesVolume = 0.0;
   double sumQuartStdDevTimesVolume = 0.0;
   for (const auto& pv : pvm_vec) {
       double standardizedDev = (static_cast<double>(pv.first) - stats.mean) / stats.stdDev;
       sumCubedStdDevTimesVolume += pv.second * std::pow(standardizedDev, 3.0);
       sumQuartStdDevTimesVolume += pv.second * std::pow(standardizedDev, 4.0);
   }

   stats.skewness = static_cast<float>(sumCubedStdDevTimesVolume / currentTotalVolume);
   float rawKurtosis = static_cast<float>(sumQuartStdDevTimesVolume / currentTotalVolume);
   stats.excessKurtosis = rawKurtosis - 3.0f;
   stats.sufficientData = true;

   return stats;
}

float CalculateVolumeProfileOverlap(const PriceVolumeMap& profile1, const PriceVolumeMap& profile2) { 
   if (profile1.empty() || profile2.empty()) return 0.0f;
   float overlapVolume = 0.0f;
   float map1TotalVolume = 0.0f; 
   for(const auto& p : profile1) map1TotalVolume += p.second.TotalVolume;
   float map2TotalVolume = 0.0f; 
   for(const auto& p : profile2) map2TotalVolume += p.second.TotalVolume;

   if (map1TotalVolume <= 0.0f && map2TotalVolume <= 0.0f) return 0.0f; // if both profiles are empty of volume.

   std::set<float> allPrices;
   for (const auto& pair : profile1) allPrices.insert(pair.first);
   for (const auto& pair : profile2) allPrices.insert(pair.first);

   for (float price : allPrices) {
       float vol1 = (profile1.count(price)) ? profile1.at(price).TotalVolume : 0.0f;
       float vol2 = (profile2.count(price)) ? profile2.at(price).TotalVolume : 0.0f;
       overlapVolume += std::min(vol1, vol2);
   }
   float unionVolume = map1TotalVolume + map2TotalVolume - overlapVolume;
   return (unionVolume > 0.00001f) ? (overlapVolume / unionVolume) * 100.0f : 0.0f;
}

float CalculateValueAreaOverlap(float VAH1, float VAL1, float VAH2, float VAL2, float TickSize) { 
   if (VAH1 < VAL1 || VAH2 < VAL2) return 0.0f;
   float vaRange1 = VAH1 - VAL1; 
   float vaRange2 = VAH2 - VAL2;
   // If both ranges are effectively zero (single price level VA), consider 100% overlap if they are the same, 0% otherwise.
   if (vaRange1 < TickSize / 2.0f && vaRange2 < TickSize / 2.0f) {
       return (std::fabs(VAL1 - VAL2) < TickSize / 2.0f) ? 100.0f : 0.0f;
   }
   // If one range is zero and the other is not, no meaningful overlap.
   if (vaRange1 < TickSize / 2.0f || vaRange2 < TickSize / 2.0f) return 0.0f;

   float overlapStart = std::max(VAL1, VAL2);
   float overlapEnd = std::min(VAH1, VAH2);
   if (overlapStart >= overlapEnd - TickSize / 2.0f) return 0.0f; // No overlap or too small
   float overlapLength = overlapEnd - overlapStart;
   float unionStart = std::min(VAL1, VAL2); 
   float unionEnd = std::max(VAH1, VAH2);
   float unionLength = unionEnd - unionStart;
   if (unionLength < TickSize / 2.0f) return 0.0f; // Union is too small
   return (overlapLength / unionLength) * 100.0f;
}

float CalculateRangeSimilarityDiff(const s_SessionProfile& profileN, const s_SessionProfile& profileN1, float tickSize) { 
   float rangeN = profileN.GetRange(); 
   float rangeN1 = profileN1.GetRange();
   if (rangeN <= tickSize / 2.0f && rangeN1 <= tickSize / 2.0f) return 0.0f; // Both effectively zero range
   if (rangeN <= tickSize / 2.0f || rangeN1 <= tickSize / 2.0f) return 200.0f; // One is zero range, max difference
   float avgRange = (rangeN + rangeN1) / 2.0f; 
   if (avgRange <= 0.00001f) return 200.0f; // Avoid division by zero
   return std::fabs(rangeN - rangeN1) / avgRange * 100.0f; 
}

bool CheckRangeSimilarity(float diffPercent, float thresholdPercent) { 
   return diffPercent <= thresholdPercent;
}

float CalculateMaxAllowedHigh(float referenceHigh, float referenceRange, float tolerancePercent, float tickSize) { 
   float rangeForTol = std::max(referenceRange, tickSize); // Ensure at least 1 tick for tolerance base
   float toleranceValue = rangeForTol * (tolerancePercent / 100.0f); 
   return referenceHigh + toleranceValue;
}

bool CheckHighPosition(float profileHigh, float maxAllowedHigh) { 
   if (maxAllowedHigh <= -FLT_MAX) return false; 
   return profileHigh <= maxAllowedHigh;
}

float CalculateMinAllowedLow(float referenceLow, float referenceRange, float tolerancePercent, float tickSize) { 
   float rangeForTol = std::max(referenceRange, tickSize); // Ensure at least 1 tick for tolerance base
   float toleranceValue = rangeForTol * (tolerancePercent / 100.0f); 
   return referenceLow - toleranceValue;
}

bool CheckLowPosition(float profileLow, float minAllowedLow) { 
   if (minAllowedLow >= FLT_MAX) return false; 
   return profileLow >= minAllowedLow;
}

PriceVolumeMap MergeMultipleVolumeProfiles(const std::vector<std::reference_wrapper<const PriceVolumeMap>>& profileMapsToMerge) { 
   PriceVolumeMap mergedProfile; 
   if (profileMapsToMerge.empty()) return mergedProfile;
   for(const auto& profileMapRef : profileMapsToMerge) { 
       const PriceVolumeMap& profileMap = profileMapRef.get();
       for (const auto& pairB : profileMap) { 
           const float price = pairB.first; 
           const s_PriceLevelVolume& volumeDataB = pairB.second;
           auto it = mergedProfile.find(price); 
           if (it != mergedProfile.end()) { 
               it->second.TotalVolume += volumeDataB.TotalVolume; 
               it->second.NumberOfTrades += volumeDataB.NumberOfTrades;
           } else { 
               mergedProfile[price] = volumeDataB; 
           } 
       } 
   } 
   return mergedProfile;
}

void CalculateProfileMetrics(const PriceVolumeMap& priceMap, float valueAreaPercentage, SCStudyInterfaceRef sc, float& poc, float& valueAreaHigh, float& valueAreaLow, float& highestPrice, float& lowestPrice, float& totalVolume) { 
   poc = 0.0f;
   valueAreaHigh = 0.0f; 
   valueAreaLow = 0.0f; 
   highestPrice = -FLT_MAX; 
   lowestPrice = FLT_MAX; 
   totalVolume = 0.0f; 
   if (priceMap.empty()) return;
   
   float maxVolumeAtPOC = 0.0f; 
   std::vector<std::pair<float, float>> priceVolPairs; 
   priceVolPairs.reserve(priceMap.size()); 
   for (const auto& pair : priceMap) { 
       const float price = pair.first;
       const float volume = pair.second.TotalVolume; 
       if (volume > 0.00001f) { 
           totalVolume += volume; 
           priceVolPairs.push_back({price, volume});
           if (volume > maxVolumeAtPOC) { 
               maxVolumeAtPOC = volume; 
               poc = price;
           } else if (std::fabs(volume - maxVolumeAtPOC) < 0.00001f && price > poc) { 
               poc = price; // Higher POC if volume is effectively same
           } 
           if (price > highestPrice) highestPrice = price; 
           if (price < lowestPrice) lowestPrice = price;
       } 
   } 
   
   if (totalVolume <= 0.00001f || priceVolPairs.empty()) {
       // If no volume, try to set H/L from map keys if they exist.
       bool first = true;
       if (!priceMap.empty()) {
           for(const auto& pair : priceMap) { // This loop runs only if priceMap is not empty.
               if (first) { 
                   highestPrice = pair.first; 
                   lowestPrice = pair.first; 
                   first = false; 
               } else { 
                   if(pair.first > highestPrice) highestPrice = pair.first; 
                   if(pair.first < lowestPrice) lowestPrice = pair.first; 
               }
           }
       }
       if(highestPrice > -FLT_MAX && lowestPrice < FLT_MAX && highestPrice >= lowestPrice) { // Check if any valid H/L was found
           valueAreaHigh = highestPrice; 
           valueAreaLow = lowestPrice;
           poc = lowestPrice + (highestPrice - lowestPrice) / 2.0f; // Midpoint as POC
       } else { // Reset if still invalid
           highestPrice = -FLT_MAX; 
           lowestPrice = FLT_MAX; 
           poc = 0.0f; 
           valueAreaHigh = 0.0f; 
           valueAreaLow = 0.0f;
       }
       return;
   } 
   
   std::sort(priceVolPairs.begin(), priceVolPairs.end(), [](const auto& a, const auto& b) { return a.first < b.first; }); 
   size_t pocIndex = 0;
   bool pocFoundInSortedList = false; 
   for (size_t i = 0; i < priceVolPairs.size(); ++i) { 
       if (std::fabs(priceVolPairs[i].first - poc) < sc.TickSize / 2.0f) { 
           pocIndex = i; 
           pocFoundInSortedList = true; 
           break; 
       } 
   }
   
   if (!pocFoundInSortedList && !priceVolPairs.empty()) { // Fallback if POC from initial scan wasn't in list (e.g. float precision issues)
       maxVolumeAtPOC = 0.0f; 
       pocIndex = 0; 
       poc = priceVolPairs[0].first;
       for(size_t i = 0; i < priceVolPairs.size(); ++i) {
           if (priceVolPairs[i].second > maxVolumeAtPOC) { 
               maxVolumeAtPOC = priceVolPairs[i].second; 
               pocIndex = i; 
               poc = priceVolPairs[i].first; 
           } else if (std::fabs(priceVolPairs[i].second - maxVolumeAtPOC) < 0.00001f && priceVolPairs[i].first > poc) { 
               pocIndex = i; 
               poc = priceVolPairs[i].first;
           }
       }
   } else if (priceVolPairs.empty()) return; // Should be caught by totalVolume check
   
   float targetVolume = totalVolume * (valueAreaPercentage / 100.0f); 
   float currentVolumeInVA = 0.0f;
   if (pocIndex < priceVolPairs.size()) { // Ensure pocIndex is valid before accessing
       currentVolumeInVA = priceVolPairs[pocIndex].second;
   } else if (!priceVolPairs.empty()) { // If pocIndex somehow invalid, default to first element
       pocIndex = 0; 
       currentVolumeInVA = priceVolPairs[pocIndex].second; 
       poc = priceVolPairs[pocIndex].first;
   } else return; // No data to process
   
   valueAreaHigh = poc; 
   valueAreaLow = poc; 
   size_t upperIndex = pocIndex; 
   size_t lowerIndex = pocIndex;
   
   while (currentVolumeInVA < targetVolume) { 
       bool canGoHigher = (upperIndex + 1 < priceVolPairs.size()); 
       bool canGoLower = (lowerIndex > 0);
       if (!canGoHigher && !canGoLower) break; 
       float higherVol = canGoHigher ? priceVolPairs[upperIndex + 1].second : 0.0f; 
       float lowerVol = canGoLower ? priceVolPairs[lowerIndex - 1].second : 0.0f;
       
       if (canGoHigher && canGoLower && higherVol > 0.00001f && std::fabs(higherVol - lowerVol) < 0.00001f) { 
           currentVolumeInVA += (higherVol + lowerVol); 
           valueAreaHigh = priceVolPairs[upperIndex + 1].first; 
           valueAreaLow = priceVolPairs[lowerIndex - 1].first; 
           upperIndex++; 
           lowerIndex--;
       } else if (canGoHigher && (!canGoLower || higherVol >= lowerVol)) { 
           if(higherVol > 0.00001f) {
               currentVolumeInVA += higherVol; 
               valueAreaHigh = priceVolPairs[upperIndex + 1].first;
           } 
           upperIndex++;
       } else if (canGoLower) { 
           if(lowerVol > 0.00001f) {
               currentVolumeInVA += lowerVol; 
               valueAreaLow = priceVolPairs[lowerIndex - 1].first;
           } 
           lowerIndex--; 
       } else break;
       
       if (lowerIndex > upperIndex && (lowerIndex !=0 || upperIndex != priceVolPairs.size()-1) ) break; 
   } 
}

// NEW: Function to check for BA activation
void CheckForBAActivation(SCStudyInterfaceRef sc, s_BAStudyPersistentData* pData, float TickSize) {
   if (pData->FinalizedBalanceAreas.empty()) return;
   
   // Check each finalized BA for activation
   for (auto& ba : pData->FinalizedBalanceAreas) {
       if (ba.IsActivated) continue; // Already activated
       
       // Look for price breaking outside the Value Area
       int checkStartIndex = ba.EndBarIndex + 1; // Start checking after BA formation ends
       if (checkStartIndex >= sc.ArraySize) continue;
       
       float tolerance = TickSize / 2.0f;
       
       // Check bars after BA formation for breakout
       for (int i = checkStartIndex; i < sc.ArraySize; ++i) {
           bool activated = false;
           
           // Check for break above Value Area High
           if (sc.High[i] > ba.ValueAreaHigh + tolerance) {
               ba.IsActivated = true;
               ba.ActivationDateTime = sc.BaseDateTimeIn[i];
               ba.ActivationBarIndex = i;
               ba.ActivationPrice = sc.High[i];
               ba.ActivationType = "Break_High";
               ba.ActivatedHigh = true;
               activated = true;
           }
           // Check for break below Value Area Low
           else if (sc.Low[i] < ba.ValueAreaLow - tolerance) {
               ba.IsActivated = true;
               ba.ActivationDateTime = sc.BaseDateTimeIn[i];
               ba.ActivationBarIndex = i;
               ba.ActivationPrice = sc.Low[i];
               ba.ActivationType = "Break_Low";
               ba.ActivatedLow = true;
               activated = true;
           }
           
           if (activated) {
               // Set up for extension
               ba.IsExtending = true;
               ba.ExtensionEndIndex = sc.ArraySize - 1; // Default to chart end
               ba.ExtensionEndReason = "Chart_End";
               
               // Add to active BAs list if not already there
               bool alreadyInActiveList = false;
               for (const auto& activeBa : pData->ActiveBalanceAreas) {
                   if (activeBa.StartProfileChronoIndex == ba.StartProfileChronoIndex && 
                       activeBa.EndProfileChronoIndex == ba.EndProfileChronoIndex) {
                       alreadyInActiveList = true;
                       break;
                   }
               }
               if (!alreadyInActiveList) {
                   pData->ActiveBalanceAreas.push_back(ba);
               }
               break; // Stop checking this BA
           }
       }
   }
}

void CheckForPBALCreation(SCStudyInterfaceRef sc, s_BAStudyPersistentData* pData, 
                         const s_BalanceArea& cutBA, const s_BalanceArea& intersectingBA, 
                         float pierceThresholdPercent, float TickSize) {
    
    float orangeBARange = cutBA.ValueAreaHigh - cutBA.ValueAreaLow;
    if (orangeBARange <= 0.0f) return;
    
    float pierceThreshold = orangeBARange * (pierceThresholdPercent / 100.0f);
    
    // Check if orange BA's VALUE AREA HIGH pierces ABOVE the intersecting blue BA's ENTIRE RANGE HIGH by threshold
    bool createHighPBAL = (cutBA.ValueAreaHigh > intersectingBA.HighestPrice + pierceThreshold);
    
    // Check if orange BA's VALUE AREA LOW pierces BELOW the intersecting blue BA's ENTIRE RANGE LOW by threshold  
    bool createLowPBAL = (cutBA.ValueAreaLow < intersectingBA.LowestPrice - pierceThreshold);
    
    // Create PBAH (high ray) only if high edge pierces above blue BA
    if (createHighPBAL) {
        s_PBALDrawingInfo pbalHigh;
        pbalHigh.StartBarIndex = cutBA.ActivationBarIndex;
        pbalHigh.EndBarIndex = sc.ArraySize - 1;
        pbalHigh.Price = cutBA.ValueAreaHigh;
        pbalHigh.IsHigh = true;
        pbalHigh.EndReason = "Chart_End";
        pbalHigh.OriginStartProfileIndex = cutBA.StartProfileChronoIndex;
        pbalHigh.OriginEndProfileIndex = cutBA.EndProfileChronoIndex;
        
        // Create origin label
        SCString dateStr;
        if (!cutBA.StartDateTime.IsUnset()) {
            int year = cutBA.StartDateTime.GetYear();
            int month = cutBA.StartDateTime.GetMonth();
            int day = cutBA.StartDateTime.GetDay();
            dateStr.Format("%02d-%02d-%02d", month, day, year % 100);
        } else {
            dateStr = "N/A";
        }
        float volumeInMillions = cutBA.TotalVolume / 1000000.0f;
        int sessionCount = (int)cutBA.IncludedProfileIndices.size();
        pbalHigh.OriginLabel.Format("PBAH %s %.2fM %dD", dateStr.GetChars(), volumeInMillions, sessionCount);
        
        pData->PBALsToDraw.push_back(pbalHigh);
    }
    
    // Create PBAL (low ray) only if low edge pierces below blue BA
    if (createLowPBAL) {
        s_PBALDrawingInfo pbalLow;
        pbalLow.StartBarIndex = cutBA.ActivationBarIndex;
        pbalLow.EndBarIndex = sc.ArraySize - 1;
        pbalLow.Price = cutBA.ValueAreaLow;
        pbalLow.IsHigh = false;
        pbalLow.EndReason = "Chart_End";
        pbalLow.OriginStartProfileIndex = cutBA.StartProfileChronoIndex;
        pbalLow.OriginEndProfileIndex = cutBA.EndProfileChronoIndex;
        
        // Create origin label
        SCString dateStr;
        if (!cutBA.StartDateTime.IsUnset()) {
            int year = cutBA.StartDateTime.GetYear();
            int month = cutBA.StartDateTime.GetMonth();
            int day = cutBA.StartDateTime.GetDay();
            dateStr.Format("%02d-%02d-%02d", month, day, year % 100);
        } else {
            dateStr = "N/A";
        }
        float volumeInMillions = cutBA.TotalVolume / 1000000.0f;
        int sessionCount = (int)cutBA.IncludedProfileIndices.size();
        pbalLow.OriginLabel.Format("PBAL %s %.2fM %dD", dateStr.GetChars(), volumeInMillions, sessionCount);
        
        pData->PBALsToDraw.push_back(pbalLow);
    }
}

// NEW: Function to check for BA intersections and update extension endpoints
// NEW: Function to check for BA intersections and update extension endpoints
void UpdateBAExtensions(SCStudyInterfaceRef sc, s_BAStudyPersistentData* pData, float TickSize, float PBALPierceThreshold) {
    // Sort active BAs by activation time for proper cutting logic
    std::sort(pData->ActiveBalanceAreas.begin(), pData->ActiveBalanceAreas.end(), 
              [](const s_BalanceArea& a, const s_BalanceArea& b) {
                  return a.ActivationBarIndex < b.ActivationBarIndex;
              });

    // Check each active BA for intersections with newly formed BAs and other active BAs
    for (size_t i = 0; i < pData->ActiveBalanceAreas.size(); ++i) {
        auto& activeBa = pData->ActiveBalanceAreas[i];
        if (!activeBa.IsExtending) continue;
        
        int earliestCutPoint = sc.ArraySize - 1; // Start with chart end
        bool shouldCut = false;
        s_BalanceArea intersectingBA;
        bool foundIntersectingBA = false;
        
        // Check against ALL finalized BAs that have been activated after this one
        for (const auto& newBa : pData->FinalizedBalanceAreas) {
            // Only check BAs that were activated after this one was activated
            if (!newBa.IsActivated || newBa.ActivationBarIndex <= activeBa.ActivationBarIndex) continue;
            
            // Check if the new activated BA's VALUE AREA range intersects with the active BA's VALUE AREA range
            float overlapStart = std::max(activeBa.ValueAreaLow, newBa.ValueAreaLow);
            float overlapEnd = std::min(activeBa.ValueAreaHigh, newBa.ValueAreaHigh);
            
            if (overlapEnd > overlapStart + TickSize / 2.0f) {
                // Value Area ranges intersect - cut at the activation point of the intersecting BA
                if (newBa.ActivationBarIndex > activeBa.ActivationBarIndex && 
                    newBa.ActivationBarIndex < earliestCutPoint) {
                    earliestCutPoint = newBa.ActivationBarIndex;
                    shouldCut = true;
                    intersectingBA = newBa;
                    foundIntersectingBA = true;
                }
            }
        }
        
        // Check against other active BAs that activated later
        for (size_t j = i + 1; j < pData->ActiveBalanceAreas.size(); ++j) {
            const auto& laterActiveBa = pData->ActiveBalanceAreas[j];
            
            // Only check BAs that activated after this one
            if (laterActiveBa.ActivationBarIndex <= activeBa.ActivationBarIndex) continue;
            
            // Check if their VALUE AREA ranges intersect
            float overlapStart = std::max(activeBa.ValueAreaLow, laterActiveBa.ValueAreaLow);
            float overlapEnd = std::min(activeBa.ValueAreaHigh, laterActiveBa.ValueAreaHigh);
            
            if (overlapEnd > overlapStart + TickSize / 2.0f) {
                // Value Area ranges intersect - cut at the activation point of the later BA
                if (laterActiveBa.ActivationBarIndex > activeBa.ActivationBarIndex && 
                    laterActiveBa.ActivationBarIndex < earliestCutPoint) {
                    earliestCutPoint = laterActiveBa.ActivationBarIndex;
                    shouldCut = true;
                    intersectingBA = laterActiveBa;
                    foundIntersectingBA = true;
                }
            }
        }
        
        // Apply the cut if needed
        if (shouldCut && earliestCutPoint < activeBa.ExtensionEndIndex) {
            activeBa.ExtensionEndIndex = earliestCutPoint;
            activeBa.ExtensionEndReason = "BA_Intersection";
            activeBa.WasCut = true;          // Mark as cut
            activeBa.IsExtending = false;    // No longer extending
            
            // Check for PBAL creation if we found the intersecting BA
            if (foundIntersectingBA) {
                CheckForPBALCreation(sc, pData, activeBa, intersectingBA, PBALPierceThreshold, TickSize);
            }
        }
    }
    
    // Update active BAs in the main list to reflect changes
    for (size_t i = 0; i < pData->FinalizedBalanceAreas.size(); ++i) {
        auto& finalizedBa = pData->FinalizedBalanceAreas[i];
        if (!finalizedBa.IsActivated) continue;
        
        // Find corresponding active BA and sync the data
        for (const auto& activeBa : pData->ActiveBalanceAreas) {
            if (activeBa.StartProfileChronoIndex == finalizedBa.StartProfileChronoIndex && 
                activeBa.EndProfileChronoIndex == finalizedBa.EndProfileChronoIndex) {
                finalizedBa.IsExtending = activeBa.IsExtending;
                finalizedBa.ExtensionEndIndex = activeBa.ExtensionEndIndex;
                finalizedBa.ExtensionEndReason = activeBa.ExtensionEndReason;
                finalizedBa.WasCut = activeBa.WasCut;
                break;
            }
        }
    }
}

float CalculateRangeOverlapPercent_RelativeToSmaller(const s_BalanceArea& ba1, const s_BalanceArea& ba2, float TickSize) { 
   if (ba1.HighestPrice <= -FLT_MAX || ba1.LowestPrice >= FLT_MAX || ba1.HighestPrice < ba1.LowestPrice || ba2.HighestPrice <= -FLT_MAX || ba2.LowestPrice >= FLT_MAX || ba2.HighestPrice < ba2.LowestPrice) return 0.0f;
   float overlapStart = std::max(ba1.LowestPrice, ba2.LowestPrice); 
   float overlapEnd = std::min(ba1.HighestPrice, ba2.HighestPrice); 
   float intersectionLength = overlapEnd - overlapStart;
   float tickTolerance = TickSize / 2.0f; 
   if (intersectionLength < tickTolerance) return 0.0f; 
   float range1 = ba1.GetRange(); 
   float range2 = ba2.GetRange();
   if (range1 < tickTolerance && range2 < tickTolerance) return 100.0f; // Both effectively zero-range and intersecting
   float smallerRange = std::min(range1, range2); 
   float referenceRange = std::max(smallerRange, TickSize); // Ensure reference is at least one tick
   if (referenceRange < tickTolerance) return 100.0f; // If smaller range is zero, and there's intersection
   float percentage = (intersectionLength / referenceRange) * 100.0f; 
   return std::max(0.0f, std::min(100.0f, percentage));
}

int CheckTemporalProximity(const s_BalanceArea& ba1, const s_BalanceArea& ba3, const std::vector<s_SessionProfile>& allSessionProfiles, const std::vector<s_BalanceArea>& allFinalizedBAs, SCStudyInterfaceRef sc) { 
   int endProfileIndexOfBA1 = ba1.EndProfileChronoIndex;
   int startProfileIndexOfBA3 = ba3.StartProfileChronoIndex; 
   if (startProfileIndexOfBA3 < 0 || endProfileIndexOfBA1 < 0 || startProfileIndexOfBA3 >= static_cast<int>(allSessionProfiles.size()) || endProfileIndexOfBA1 >= static_cast<int>(allSessionProfiles.size())) return -1;
   if (startProfileIndexOfBA3 <= endProfileIndexOfBA1 + 1) return 0; 
   std::set<int> allUsedProfileIndicesInBetween;
   for (const auto& finalizedBA : allFinalizedBAs) {
       if (&finalizedBA == &ba1 || &finalizedBA == &ba3) continue; // Don't check against self
       //Only consider BAs that could potentially fall between ba1 and ba3
       if (finalizedBA.StartProfileChronoIndex >= startProfileIndexOfBA3 || finalizedBA.EndProfileChronoIndex <= endProfileIndexOfBA1) continue;
       for (int profileIndex : finalizedBA.IncludedProfileIndices) { 
           if (profileIndex > endProfileIndexOfBA1 && profileIndex < startProfileIndexOfBA3) { 
               allUsedProfileIndicesInBetween.insert(profileIndex); 
           } 
       } 
   }
   int unattributedGapCount = 0; 
   for (int p = endProfileIndexOfBA1 + 1; p < startProfileIndexOfBA3; ++p) { 
       if (allUsedProfileIndicesInBetween.count(p) == 0) { 
           unattributedGapCount++; 
       } 
   } 
   return unattributedGapCount; 
}

// --- Main Study Function ---
SCSFExport scsf_BalanceAreaDetection(SCStudyInterfaceRef sc) {
   const int BA_RECTANGLE_BASE = 80000;
   const int BA_VA_LINE_BASE = 85000; 
   const int BA_LABEL_BASE = 90000; 
   const int PROBE_LINE_BASE = 95000;
   const int COMP_BA_RECT_BASE = 100000;
   
   // Input Indices (Matches original structure)
   const int IN_VAP_STUDY_REF = 0; 
   const int IN_NUM_SESSIONS = 1;
   const int IN_MIN_VOL_OVERLAP = 2; 
   const int IN_VA_PERCENTAGE = 3; 
   const int IN_VAP_TICK_MULTIPLIER = 4; 
   const int IN_MIN_VA_OVERLAP = 5;
   const int IN_RANGE_SIMILARITY_PERCENT = 6; 
   const int IN_HIGH_LOW_TOLERANCE_PERCENT = 7;
   const int IN_DRAW_RECTANGLES = 8; 
   const int IN_RECT_BORDER_COLOR = 9;
   const int IN_RECT_FILL_COLOR = 10; 
   const int IN_RECT_TRANSPARENCY = 11; 
   const int IN_RECT_BORDER_WIDTH = 12;
   const int IN_DRAW_VA_LINES = 13;
   const int IN_VAH_COLOR = 14; 
   const int IN_VAL_COLOR = 15; 
   const int IN_VA_LINE_WIDTH = 16; 
   const int IN_VA_LINE_STYLE = 17;
   const int IN_SHOW_LABEL = 18; 
   const int IN_LABEL_FONT_SIZE = 19;
   const int IN_DRAW_PROBE_LINES = 20; 
   const int IN_HIGH_PROBE_COLOR = 21;
   const int IN_LOW_PROBE_COLOR = 22; 
   const int IN_PROBE_LINE_WIDTH = 23; 
   const int IN_PROBE_LINE_STYLE = 24; 
   const int IN_EXTEND_PROBE_LINES = 25;
   const int IN_DRAW_COMPOSITE_RECT = 26; 
   const int IN_COMP_RECT_BORDER_COLOR = 27; 
   const int IN_COMP_RECT_FILL_COLOR = 28; 
   const int IN_COMP_RECT_TRANSPARENCY = 29;
   const int IN_COMP_RECT_BORDER_WIDTH = 30;
   const int IN_RANGE_CONT_PERCENT = 31;
   const int IN_DEBUG_COMPOSITE_BA = 32;
   const int IN_DEBUG_BA_FORMATION = 33;
   // NEW Normality Filter Inputs
   const int IN_FILTER_BY_NORMALITY = 34;
   const int IN_MAX_ABS_SKEWNESS = 35;
   const int IN_MIN_EXCESS_KURTOSIS = 36;
   const int IN_MAX_EXCESS_KURTOSIS = 37;
   const int IN_ALLOW_USER_ADJUSTMENT = 38;
   const int IN_DRAW_ACTIVE_BAS = 39;
    const int IN_PBAL_PIERCE_THRESHOLD = 40;
	// ADD THESE NEW INPUTS:
	const int IN_ACTIVE_RECT_BORDER_COLOR = 41;
	const int IN_ACTIVE_RECT_FILL_COLOR = 42;
	const int IN_ACTIVE_RECT_TRANSPARENCY = 43;
	const int IN_ACTIVE_RECT_BORDER_WIDTH = 44;
	const int IN_ACTIVE_SHOW_LABEL = 45;
	const int IN_ACTIVE_LABEL_FONT_SIZE = 46;

   if (sc.SetDefaults) { 
       sc.GraphName = "Auto BAs";
       sc.StudyDescription = "Identifies BAs & Composite BAs. Draws BAs, VAs, Probes, and optional Composite BA rectangles. Includes normality filter for BAs.";
       sc.AutoLoop = 0; 
       sc.UpdateAlways = 1; 
       sc.GraphRegion = 0;
       
       sc.Input[IN_VAP_STUDY_REF].Name = "Volume by Price Study Reference"; 
       sc.Input[IN_VAP_STUDY_REF].SetStudyID(2);
       sc.Input[IN_NUM_SESSIONS].Name = "Number of Sessions to Track"; 
       sc.Input[IN_NUM_SESSIONS].SetInt(250); 
       sc.Input[IN_NUM_SESSIONS].SetIntLimits(2, 500);
       sc.Input[IN_MIN_VOL_OVERLAP].Name = "Minimum Volume Overlap % (Initiation/Extension)"; 
       sc.Input[IN_MIN_VOL_OVERLAP].SetFloat(25.0f); 
       sc.Input[IN_MIN_VOL_OVERLAP].SetFloatLimits(0.1f, 100.0f);
       sc.Input[IN_VA_PERCENTAGE].Name = "Value Area Percentage (For VA Overlap & Drawing)"; 
       sc.Input[IN_VA_PERCENTAGE].SetFloat(70.0f); 
       sc.Input[IN_VA_PERCENTAGE].SetFloatLimits(1.0f, 100.0f);
       sc.Input[IN_VAP_TICK_MULTIPLIER].Name = "Price Tick Multiplier (Profile Source)"; 
       sc.Input[IN_VAP_TICK_MULTIPLIER].SetInt(1); 
       sc.Input[IN_VAP_TICK_MULTIPLIER].SetIntLimits(1, 1000);
       sc.Input[IN_MIN_VA_OVERLAP].Name = "Minimum VA Overlap % (Initiation)"; 
       sc.Input[IN_MIN_VA_OVERLAP].SetFloat(50.0f); 
       sc.Input[IN_MIN_VA_OVERLAP].SetFloatLimits(0.1f, 100.0f);
       sc.Input[IN_RANGE_SIMILARITY_PERCENT].Name = "Max Range Diff % (Geometric Initiation)"; 
       sc.Input[IN_RANGE_SIMILARITY_PERCENT].SetFloat(30.0f); 
       sc.Input[IN_RANGE_SIMILARITY_PERCENT].SetFloatLimits(0.1f, 200.0f);
       sc.Input[IN_HIGH_LOW_TOLERANCE_PERCENT].Name = "High/Low Tolerance % (Geometric Init/Ext)"; 
       sc.Input[IN_HIGH_LOW_TOLERANCE_PERCENT].SetFloat(10.0f); 
       sc.Input[IN_HIGH_LOW_TOLERANCE_PERCENT].SetFloatLimits(0.1f, 100.0f);
       sc.Input[IN_DRAW_RECTANGLES].Name = "Draw Balance Area Rectangles"; 
       sc.Input[IN_DRAW_RECTANGLES].SetYesNo(1);
       sc.Input[IN_RECT_BORDER_COLOR].Name = "Rectangle Border Color"; 
       sc.Input[IN_RECT_BORDER_COLOR].SetColor(RGB(0, 128, 255));
       sc.Input[IN_RECT_FILL_COLOR].Name = "Rectangle Fill Color"; 
       sc.Input[IN_RECT_FILL_COLOR].SetColor(RGB(0, 128, 255));
       sc.Input[IN_RECT_TRANSPARENCY].Name = "Rectangle Transparency (0-100)"; 
       sc.Input[IN_RECT_TRANSPARENCY].SetInt(40); 
       sc.Input[IN_RECT_TRANSPARENCY].SetIntLimits(0, 100);
       sc.Input[IN_RECT_BORDER_WIDTH].Name = "Rectangle Border Width"; 
       sc.Input[IN_RECT_BORDER_WIDTH].SetInt(1); 
       sc.Input[IN_RECT_BORDER_WIDTH].SetIntLimits(1, 10);
       sc.Input[IN_SHOW_LABEL].Name = "Show BA Info Label"; 
       sc.Input[IN_SHOW_LABEL].SetYesNo(1);
       sc.Input[IN_LABEL_FONT_SIZE].Name = "Label Font Size"; 
       sc.Input[IN_LABEL_FONT_SIZE].SetInt(9); 
       sc.Input[IN_LABEL_FONT_SIZE].SetIntLimits(7, 20);
       sc.Input[IN_DRAW_PROBE_LINES].Name = "Draw Probe Lines"; 
       sc.Input[IN_DRAW_PROBE_LINES].SetYesNo(0);
       sc.Input[IN_HIGH_PROBE_COLOR].Name = "High Probe Line Color"; 
       sc.Input[IN_HIGH_PROBE_COLOR].SetColor(RGB(0, 255, 0));
       sc.Input[IN_LOW_PROBE_COLOR].Name = "Low Probe Line Color"; 
       sc.Input[IN_LOW_PROBE_COLOR].SetColor(RGB(255, 0, 0));
       sc.Input[IN_PROBE_LINE_WIDTH].Name = "Probe Line Width"; 
       sc.Input[IN_PROBE_LINE_WIDTH].SetInt(1); 
       sc.Input[IN_PROBE_LINE_WIDTH].SetIntLimits(1, 5);
       sc.Input[IN_PROBE_LINE_STYLE].Name = "Probe Line Style"; 
       sc.Input[IN_PROBE_LINE_STYLE].SetCustomInputStrings("SOLID;DASH;DOT;DASHDOT;DASHDOTDOT"); 
       sc.Input[IN_PROBE_LINE_STYLE].SetInt(LINESTYLE_DOT);
       sc.Input[IN_EXTEND_PROBE_LINES].Name = "Extend Probe Lines to Intersection"; 
       sc.Input[IN_EXTEND_PROBE_LINES].SetYesNo(1);
       sc.Input[IN_DRAW_COMPOSITE_RECT].Name = "Draw Composite BA Rectangles"; 
       sc.Input[IN_DRAW_COMPOSITE_RECT].SetYesNo(0);
       sc.Input[IN_COMP_RECT_BORDER_COLOR].Name = "Composite Rect Border Color"; 
       sc.Input[IN_COMP_RECT_BORDER_COLOR].SetColor(RGB(255, 0, 255));
       sc.Input[IN_COMP_RECT_FILL_COLOR].Name = "Composite Rect Fill Color"; 
       sc.Input[IN_COMP_RECT_FILL_COLOR].SetColor(RGB(255, 0, 255));
       sc.Input[IN_COMP_RECT_TRANSPARENCY].Name = "Composite Rect Transparency (0-100)"; 
       sc.Input[IN_COMP_RECT_TRANSPARENCY].SetInt(80); 
       sc.Input[IN_COMP_RECT_TRANSPARENCY].SetIntLimits(0, 100);
       sc.Input[IN_COMP_RECT_BORDER_WIDTH].Name = "Composite Rect Border Width"; 
       sc.Input[IN_COMP_RECT_BORDER_WIDTH].SetInt(2); 
       sc.Input[IN_COMP_RECT_BORDER_WIDTH].SetIntLimits(1, 10);
       sc.Input[IN_RANGE_CONT_PERCENT].Name = "Range Containment Tolerance % (Comp BA)"; 
       sc.Input[IN_RANGE_CONT_PERCENT].SetFloat(35.0f); 
       sc.Input[IN_RANGE_CONT_PERCENT].SetFloatLimits(0.1f, 100.0f);
       sc.Input[IN_DEBUG_COMPOSITE_BA].Name = "Debug Mode (Composite BA Logging)"; 
       sc.Input[IN_DEBUG_COMPOSITE_BA].SetYesNo(0);
       sc.Input[IN_DEBUG_BA_FORMATION].Name = "Debug Mode (BA Formation Details)"; 
       sc.Input[IN_DEBUG_BA_FORMATION].SetYesNo(0);
       sc.Input[IN_FILTER_BY_NORMALITY].Name = "Filter BAs by Normality"; 
       sc.Input[IN_FILTER_BY_NORMALITY].SetYesNo(1); 
       sc.Input[IN_MAX_ABS_SKEWNESS].Name = "Max Abs Skewness (Normality)"; 
       sc.Input[IN_MAX_ABS_SKEWNESS].SetFloat(5.0f); 
       sc.Input[IN_MAX_ABS_SKEWNESS].SetFloatLimits(0.0f, 5.0f);
       sc.Input[IN_MIN_EXCESS_KURTOSIS].Name = "Min Excess Kurtosis (Normality)"; 
       sc.Input[IN_MIN_EXCESS_KURTOSIS].SetFloat(-0.5f); 
       sc.Input[IN_MIN_EXCESS_KURTOSIS].SetFloatLimits(-2.0f, 10.0f);
       sc.Input[IN_MAX_EXCESS_KURTOSIS].Name = "Max Excess Kurtosis (Normality)"; 
       sc.Input[IN_MAX_EXCESS_KURTOSIS].SetFloat(5.0f); 
       sc.Input[IN_MAX_EXCESS_KURTOSIS].SetFloatLimits(-2.0f, 20.0f);
       sc.Input[IN_ALLOW_USER_ADJUSTMENT].Name = "Allow Manual Adjustment of Drawings";
       sc.Input[IN_ALLOW_USER_ADJUSTMENT].SetYesNo(1); // Default to YES	
        sc.Input[IN_DRAW_ACTIVE_BAS].Name = "Draw Active Balance Areas"; 
        sc.Input[IN_DRAW_ACTIVE_BAS].SetYesNo(1); // Default to YES
        sc.Input[IN_PBAL_PIERCE_THRESHOLD].Name = "PBAL Pierce Threshold %";
        sc.Input[IN_PBAL_PIERCE_THRESHOLD].SetFloat(15.0f);
        sc.Input[IN_PBAL_PIERCE_THRESHOLD].SetFloatLimits(1.0f, 50.0f);
        sc.Input[IN_ACTIVE_RECT_BORDER_COLOR].Name = "Active BA Rectangle Border Color";
        sc.Input[IN_ACTIVE_RECT_BORDER_COLOR].SetColor(RGB(255, 165, 0)); // Orange
        sc.Input[IN_ACTIVE_RECT_FILL_COLOR].Name = "Active BA Rectangle Fill Color";
        sc.Input[IN_ACTIVE_RECT_FILL_COLOR].SetColor(RGB(255, 165, 0)); // Orange
        sc.Input[IN_ACTIVE_RECT_TRANSPARENCY].Name = "Active BA Rectangle Transparency (0-100)";
        sc.Input[IN_ACTIVE_RECT_TRANSPARENCY].SetInt(65);
        sc.Input[IN_ACTIVE_RECT_TRANSPARENCY].SetIntLimits(0, 100);
        sc.Input[IN_ACTIVE_RECT_BORDER_WIDTH].Name = "Active BA Rectangle Border Width";
        sc.Input[IN_ACTIVE_RECT_BORDER_WIDTH].SetInt(1);
        sc.Input[IN_ACTIVE_RECT_BORDER_WIDTH].SetIntLimits(1, 10);
        sc.Input[IN_ACTIVE_SHOW_LABEL].Name = "Show Active BA Labels";
        sc.Input[IN_ACTIVE_SHOW_LABEL].SetYesNo(1);
        sc.Input[IN_ACTIVE_LABEL_FONT_SIZE].Name = "Active BA Label Font Size";
        sc.Input[IN_ACTIVE_LABEL_FONT_SIZE].SetInt(9);
        sc.Input[IN_ACTIVE_LABEL_FONT_SIZE].SetIntLimits(7, 20);
       return;
   }
   
// --- Read Inputs (Matches original structure) ---
   int ReferenceStudyID = sc.Input[IN_VAP_STUDY_REF].GetStudyID(); 
   int NumberOfSessions = sc.Input[IN_NUM_SESSIONS].GetInt(); 
   float MinVolOverlap = sc.Input[IN_MIN_VOL_OVERLAP].GetFloat(); 
   float ValueAreaPercentage = sc.Input[IN_VA_PERCENTAGE].GetFloat();
   int PriceTickMultiplier = sc.Input[IN_VAP_TICK_MULTIPLIER].GetInt(); 
   float MinVAOverlap = sc.Input[IN_MIN_VA_OVERLAP].GetFloat(); 
   float RangeSimilarityPercent = sc.Input[IN_RANGE_SIMILARITY_PERCENT].GetFloat(); 
   float HighLowTolerancePercent = sc.Input[IN_HIGH_LOW_TOLERANCE_PERCENT].GetFloat();
   bool DrawRectangles = sc.Input[IN_DRAW_RECTANGLES].GetYesNo();
   COLORREF RectBorderColor = sc.Input[IN_RECT_BORDER_COLOR].GetColor(); 
   COLORREF RectFillColor = sc.Input[IN_RECT_FILL_COLOR].GetColor(); 
   int RectTransparency = sc.Input[IN_RECT_TRANSPARENCY].GetInt(); 
   int RectBorderWidth = sc.Input[IN_RECT_BORDER_WIDTH].GetInt();
   int LabelFontSize = sc.Input[IN_LABEL_FONT_SIZE].GetInt();
   bool DrawProbeLines = sc.Input[IN_DRAW_PROBE_LINES].GetYesNo(); 
   COLORREF HighProbeColor = sc.Input[IN_HIGH_PROBE_COLOR].GetColor(); 
   COLORREF LowProbeColor = sc.Input[IN_LOW_PROBE_COLOR].GetColor(); 
   int ProbeLineWidth = sc.Input[IN_PROBE_LINE_WIDTH].GetInt();
   SubgraphLineStyles ProbeLineStyle = static_cast<SubgraphLineStyles>(sc.Input[IN_PROBE_LINE_STYLE].GetInt()); 
   bool ExtendProbeLines = sc.Input[IN_EXTEND_PROBE_LINES].GetYesNo();
   bool DrawCompositeRect = sc.Input[IN_DRAW_COMPOSITE_RECT].GetYesNo(); 
   COLORREF CompRectBorderColor = sc.Input[IN_COMP_RECT_BORDER_COLOR].GetColor(); 
   COLORREF CompRectFillColor = sc.Input[IN_COMP_RECT_FILL_COLOR].GetColor();
   int CompRectTransparency = sc.Input[IN_COMP_RECT_TRANSPARENCY].GetInt(); 
   int CompRectBorderWidth = sc.Input[IN_COMP_RECT_BORDER_WIDTH].GetInt();
   float RangeContainmentPercent = sc.Input[IN_RANGE_CONT_PERCENT].GetFloat();
   bool DebugCompositeBA = sc.Input[IN_DEBUG_COMPOSITE_BA].GetYesNo();
   bool DebugBAFormation = sc.Input[IN_DEBUG_BA_FORMATION].GetYesNo();
   bool FilterByNormality = sc.Input[IN_FILTER_BY_NORMALITY].GetYesNo();
   float MaxAbsSkewness = sc.Input[IN_MAX_ABS_SKEWNESS].GetFloat();
   float MinExcessKurtosis = sc.Input[IN_MIN_EXCESS_KURTOSIS].GetFloat();
   float MaxExcessKurtosis = sc.Input[IN_MAX_EXCESS_KURTOSIS].GetFloat();
   bool ShowLabel = sc.Input[IN_SHOW_LABEL].GetYesNo();
   bool AllowUserAdjustment = sc.Input[IN_ALLOW_USER_ADJUSTMENT].GetYesNo();
    bool DrawActiveBAs = sc.Input[IN_DRAW_ACTIVE_BAS].GetYesNo();
    float PBALPierceThreshold = sc.Input[IN_PBAL_PIERCE_THRESHOLD].GetFloat();
    COLORREF ActiveRectBorderColor = sc.Input[IN_ACTIVE_RECT_BORDER_COLOR].GetColor();
    COLORREF ActiveRectFillColor = sc.Input[IN_ACTIVE_RECT_FILL_COLOR].GetColor();
    int ActiveRectTransparency = sc.Input[IN_ACTIVE_RECT_TRANSPARENCY].GetInt();
    int ActiveRectBorderWidth = sc.Input[IN_ACTIVE_RECT_BORDER_WIDTH].GetInt();
    bool ActiveShowLabel = sc.Input[IN_ACTIVE_SHOW_LABEL].GetYesNo();
    int ActiveLabelFontSize = sc.Input[IN_ACTIVE_LABEL_FONT_SIZE].GetInt();

   float TickSize = sc.TickSize; 
   SCString logMsg;

   if (ReferenceStudyID <= 0) { 
       sc.AddMessageToLog("Error: Set Volume by Price Study Reference", 1); 
       return; 
   }
   if (TickSize <= 0.0f) { 
       sc.AddMessageToLog("Error: TickSize is zero or negative.", 1); 
       return; 
   }

   s_BAStudyPersistentData* pData = reinterpret_cast<s_BAStudyPersistentData*>(sc.GetPersistentPointer(0));
   if (pData == nullptr) { 
       pData = new s_BAStudyPersistentData; 
       sc.SetPersistentPointer(0, pData); 
   }

   // CRITICAL: Handle study removal (sc.LastCallToFunction) - MUST BE EARLY
   if (sc.LastCallToFunction) {
       // Delete all user-drawn drawings created by this study
       if (AllowUserAdjustment) {
           for (int lineNum : pData->CreatedBADrawings) {
               sc.DeleteUserDrawnACSDrawing(sc.ChartNumber, lineNum);
           }
           for (int lineNum : pData->CreatedProbeDrawings) {
               sc.DeleteUserDrawnACSDrawing(sc.ChartNumber, lineNum);
           }
           for (int lineNum : pData->CreatedCompositeDrawings) {
               sc.DeleteUserDrawnACSDrawing(sc.ChartNumber, lineNum);
           }
           for (int lineNum : pData->CreatedLabelDrawings) {
               sc.DeleteUserDrawnACSDrawing(sc.ChartNumber, lineNum);
           }
            // Delete active BA drawings
            for (const auto& activeBa : pData->ActiveBalanceAreas) {
                int extLineNum = 50000 + activeBa.StartProfileChronoIndex * 100 + activeBa.EndProfileChronoIndex;
                sc.DeleteUserDrawnACSDrawing(sc.ChartNumber, extLineNum);
                // No separate label deletion needed - embedded in rectangle
            }
            // Delete PBAL drawings
            for (const auto& pbal : pData->PBALsToDraw) {
                int pbalLineNum = 60000 + pbal.OriginStartProfileIndex * 100 + pbal.OriginEndProfileIndex + (pbal.IsHigh ? 50 : 0);
                sc.DeleteUserDrawnACSDrawing(sc.ChartNumber, pbalLineNum);
            }
       }
       
       // Clear tracking vectors
       pData->CreatedBADrawings.clear();
       pData->CreatedProbeDrawings.clear();
       pData->CreatedCompositeDrawings.clear();
       pData->CreatedLabelDrawings.clear();
       pData->UserAdjustedDrawings.clear();
       pData->ActiveBalanceAreas.clear();
       pData->CreatedActiveBADrawings.clear();
       
       return; // Exit early on study removal
   }

   // Delete ACS chart drawings (for non-user drawn mode)
   sc.DeleteACSChartDrawing(sc.ChartNumber, TOOL_DELETE_ALL, BA_RECTANGLE_BASE);
   sc.DeleteACSChartDrawing(sc.ChartNumber, TOOL_DELETE_ALL, BA_VA_LINE_BASE);
   sc.DeleteACSChartDrawing(sc.ChartNumber, TOOL_DELETE_ALL, BA_LABEL_BASE);
   sc.DeleteACSChartDrawing(sc.ChartNumber, TOOL_DELETE_ALL, PROBE_LINE_BASE);
   sc.DeleteACSChartDrawing(sc.ChartNumber, TOOL_DELETE_ALL, COMP_BA_RECT_BASE);

   // Load session profiles
   std::vector<s_SessionProfile> SessionProfiles; 
   SessionProfiles.reserve(NumberOfSessions); 
   bool profilesLoaded = false;
   
   for (int fetchIndex = NumberOfSessions - 1; fetchIndex >= 0; --fetchIndex) {
       n_ACSIL::s_StudyProfileInformation profileInfo;
       if (sc.GetStudyProfileInformation(ReferenceStudyID, fetchIndex, profileInfo)) {
           s_SessionProfile sessionProfile;
           sessionProfile.StartDateTime = profileInfo.m_StartDateTime; 
           sessionProfile.EndDateTime = profileInfo.m_EndDateTime;
           sessionProfile.BeginIndex = profileInfo.m_BeginIndex; 
           sessionProfile.EndIndex = profileInfo.m_EndIndex;
           sessionProfile.ChronologicalIndex = NumberOfSessions - 1 - fetchIndex;
           // Initialize H/L, will be overridden by CalculateProfileMetrics or PriceMap iteration
           sessionProfile.HighestPrice = -FLT_MAX; 
           sessionProfile.LowestPrice = FLT_MAX;

           int numPriceLevels = sc.GetNumPriceLevelsForStudyProfile(ReferenceStudyID, fetchIndex);
           for (int priceIndex = 0; priceIndex < numPriceLevels; priceIndex++) {
               s_VolumeAtPriceV2 vap;
               if (sc.GetVolumeAtPriceDataForStudyProfile(ReferenceStudyID, fetchIndex, priceIndex, vap) == 1 && vap.Volume > 0) {
                   s_PriceLevelVolume plv;
                   float actualPrice = sc.RoundToTickSize(static_cast<float>(vap.PriceInTicks * TickSize * PriceTickMultiplier), TickSize);
                   plv.Price = actualPrice; 
                   plv.TotalVolume = static_cast<float>(vap.Volume); 
                   plv.NumberOfTrades = vap.NumberOfTrades;
                   auto it = sessionProfile.PriceMap.find(plv.Price);
                   if (it != sessionProfile.PriceMap.end()) {
                       it->second.TotalVolume += plv.TotalVolume; 
                       it->second.NumberOfTrades += plv.NumberOfTrades;
                   } else { 
                       sessionProfile.PriceMap[plv.Price] = plv; 
                   }
               }
           }
           
           // Calculate metrics based on the populated PriceMap
           if(!sessionProfile.PriceMap.empty()) {
               float calcPOC, calcVAH, calcVAL, calcHigh, calcLow, calcVol;
               CalculateProfileMetrics(sessionProfile.PriceMap, ValueAreaPercentage, sc, calcPOC, calcVAH, calcVAL, calcHigh, calcLow, calcVol);
               sessionProfile.POC = calcPOC; 
               sessionProfile.ValueAreaHigh = calcVAH; 
               sessionProfile.ValueAreaLow = calcVAL;
               sessionProfile.TotalVolume = calcVol; 
               sessionProfile.HighestPrice = calcHigh; 
               sessionProfile.LowestPrice = calcLow;
           } else { // No volume data from profile, try to get H/L from chart bars
               sessionProfile.POC = 0.0f; 
               sessionProfile.ValueAreaHigh = 0.0f; 
               sessionProfile.ValueAreaLow = 0.0f; 
               sessionProfile.TotalVolume = 0.0f;
               if (sessionProfile.BeginIndex >= 0 && sessionProfile.EndIndex >= sessionProfile.BeginIndex && sessionProfile.EndIndex < sc.ArraySize) {
                   sessionProfile.HighestPrice = sc.GetHighest(sc.High, sessionProfile.BeginIndex, sessionProfile.EndIndex);
                   sessionProfile.LowestPrice = sc.GetLowest(sc.Low, sessionProfile.BeginIndex, sessionProfile.EndIndex);
                   if (sessionProfile.HighestPrice < sessionProfile.LowestPrice || sessionProfile.HighestPrice <= -FLT_MAX || sessionProfile.LowestPrice >= FLT_MAX) { // Invalid range
                       sessionProfile.HighestPrice = -FLT_MAX; 
                       sessionProfile.LowestPrice = FLT_MAX;
                   }
               } else { // Invalid bar indices
                   sessionProfile.HighestPrice = -FLT_MAX; 
                   sessionProfile.LowestPrice = FLT_MAX;
               }
           }
           SessionProfiles.push_back(sessionProfile); 
           profilesLoaded = true;
       } else { 
           logMsg.Format("Failed to get Profile Info for fetchIndex %d.", fetchIndex); 
           sc.AddMessageToLog(logMsg, 1); 
       }
   }
   
   if (!profilesLoaded && NumberOfSessions > 0) { 
       /* Only return if we expected profiles but got none */ 
       return; 
   }
   int numProfilesCollected = static_cast<int>(SessionProfiles.size());

   // Check if recalculation is needed
   bool needRecalculation = sc.IsFullRecalculation ||
       pData->LastProfileCount != numProfilesCollected || 
       pData->LastNumberOfSessions != NumberOfSessions ||
       pData->LastReferenceStudyID != ReferenceStudyID ||
       std::fabs(pData->LastVAPercentage - ValueAreaPercentage) > 0.001f ||
       std::fabs(pData->LastMinVolOverlap - MinVolOverlap) > 0.001f ||
       std::fabs(pData->LastMinVAOverlap - MinVAOverlap) > 0.001f ||
       std::fabs(pData->LastRangeSimilarityPercent - RangeSimilarityPercent) > 0.001f ||
       std::fabs(pData->LastHighLowTolerancePercent - HighLowTolerancePercent) > 0.001f ||
       std::fabs(pData->LastRangeContPercent - RangeContainmentPercent) > 0.001f ||
       pData->LastDrawProbeLines != DrawProbeLines || 
       pData->LastHighProbeColor != HighProbeColor || 
       pData->LastLowProbeColor != LowProbeColor ||
       pData->LastProbeLineWidth != ProbeLineWidth || 
       pData->LastProbeLineStyle != ProbeLineStyle || 
       pData->LastExtendProbeLines != ExtendProbeLines ||
       pData->LastDrawCompositeRect != DrawCompositeRect || 
       pData->LastDebugCompositeBA != DebugCompositeBA || 
       pData->LastDebugBAFormation != DebugBAFormation ||
       pData->LastFilterByNormality != FilterByNormality ||
       std::fabs(pData->LastMaxAbsSkewness - MaxAbsSkewness) > 0.001f ||
       std::fabs(pData->LastMinExcessKurtosis - MinExcessKurtosis) > 0.001f ||
       std::fabs(pData->LastMaxExcessKurtosis - MaxExcessKurtosis) > 0.001f ||
       pData->LastAllowUserAdjustment != AllowUserAdjustment;
        pData->LastAllowUserAdjustment != AllowUserAdjustment ||
        pData->LastDrawActiveBAs != DrawActiveBAs;
	   
	if (needRecalculation) {
		   // Delete all existing drawings (both user and non-user drawn)
		   if (AllowUserAdjustment) {
			   // Delete user-drawn drawings
			   for (int lineNum : pData->CreatedBADrawings) {
				   sc.DeleteUserDrawnACSDrawing(sc.ChartNumber, lineNum);
			   }
			   for (int lineNum : pData->CreatedProbeDrawings) {
				   sc.DeleteUserDrawnACSDrawing(sc.ChartNumber, lineNum);
			   }
			   for (int lineNum : pData->CreatedCompositeDrawings) {
				   sc.DeleteUserDrawnACSDrawing(sc.ChartNumber, lineNum);
			   }
			   for (int lineNum : pData->CreatedLabelDrawings) {
				   sc.DeleteUserDrawnACSDrawing(sc.ChartNumber, lineNum);
			   }
			   // Delete active BA drawings
				for (const auto& activeBa : pData->ActiveBalanceAreas) {
					int extLineNum = 50000 + activeBa.StartProfileChronoIndex * 100 + activeBa.EndProfileChronoIndex;
					sc.DeleteUserDrawnACSDrawing(sc.ChartNumber, extLineNum);
					// No separate label deletion needed - embedded in rectangle
				}
		   } else {
			   // Delete non-user drawn drawings
			   sc.DeleteACSChartDrawing(sc.ChartNumber, TOOL_DELETE_ALL, 0);
		   }
		   
		   // Clear tracking vectors and reset state
		   pData->CreatedBADrawings.clear();
		   pData->CreatedProbeDrawings.clear();
		   pData->CreatedCompositeDrawings.clear();
		   pData->CreatedLabelDrawings.clear();
		   pData->UserAdjustedDrawings.clear();
		   
		   // Update persistent data with current settings
		   pData->LastAllowUserAdjustment = AllowUserAdjustment;
		   pData->LastProfileCount = numProfilesCollected; 
		   pData->LastNumberOfSessions = NumberOfSessions; 
		   pData->LastReferenceStudyID = ReferenceStudyID; 
		   pData->LastVAPercentage = ValueAreaPercentage;
		   pData->LastMinVolOverlap = MinVolOverlap; 
		   pData->LastMinVAOverlap = MinVAOverlap; 
		   pData->LastRangeSimilarityPercent = RangeSimilarityPercent; 
		   pData->LastHighLowTolerancePercent = HighLowTolerancePercent; 
		   pData->LastRangeContPercent = RangeContainmentPercent; 
		   pData->LastDrawProbeLines = DrawProbeLines;
		   pData->LastHighProbeColor = HighProbeColor; 
		   pData->LastLowProbeColor = LowProbeColor; 
		   pData->LastProbeLineWidth = ProbeLineWidth; 
		   pData->LastProbeLineStyle = ProbeLineStyle; 
		   pData->LastExtendProbeLines = ExtendProbeLines;
		   pData->LastDrawCompositeRect = DrawCompositeRect; 
		   pData->LastDebugCompositeBA = DebugCompositeBA; 
		   pData->LastDebugBAFormation = DebugBAFormation;
		   pData->LastFilterByNormality = FilterByNormality; 
		   pData->LastMaxAbsSkewness = MaxAbsSkewness; 
		   pData->LastMinExcessKurtosis = MinExcessKurtosis; 
		   pData->LastMaxExcessKurtosis = MaxExcessKurtosis;
           pData->LastDrawActiveBAs = DrawActiveBAs;

		   pData->FinalizedBalanceAreas.clear();
		   pData->ProbeLinesToDraw.clear();
		   pData->CompositeBAs.clear();
		   pData->ActiveBalanceAreas.clear();
		   pData->CreatedActiveBADrawings.clear();
		   pData->PBALsToDraw.clear();

		   // Balance Area Formation Logic (original logic preserved)
		   std::vector<bool> profileUsed(numProfilesCollected, false);
		   for (int i = 0; i < numProfilesCollected; ++i) {
			   if (profileUsed[i]) continue;
			   if (i + 1 >= numProfilesCollected) break;
			   const s_SessionProfile& profile_i = SessionProfiles[i];
			   const s_SessionProfile& profile_i1 = SessionProfiles[i+1];
			   
			   // Check for valid H/L in profiles before using them
			   if (profile_i.HighestPrice <= -FLT_MAX || profile_i.LowestPrice >= FLT_MAX || profile_i.HighestPrice < profile_i.LowestPrice ||
				   profile_i1.HighestPrice <= -FLT_MAX || profile_i1.LowestPrice >= FLT_MAX || profile_i1.HighestPrice < profile_i1.LowestPrice ) {
				   if (DebugBAFormation) {
						logMsg.Format("DEBUG BA: Skipping initiation at profile %d. Invalid data in profile %d (H:%.2f L:%.2f R:%.2f) or %d (H:%.2f L:%.2f R:%.2f).", i, i, profile_i.HighestPrice, profile_i.LowestPrice, profile_i.GetRange(), i+1, profile_i1.HighestPrice, profile_i1.LowestPrice, profile_i1.GetRange());
						sc.AddMessageToLog(logMsg,0);
				   }
				   continue;
			   }

			   bool startBA = false; 
			   std::string initiationReason = "None";
			   float volOverlap_i_i1 = CalculateVolumeProfileOverlap(profile_i.PriceMap, profile_i1.PriceMap);
			   if (volOverlap_i_i1 >= MinVolOverlap) { 
				   startBA = true; 
				   initiationReason = "Volume Overlap"; 
			   }
			   
			   if (!startBA) {
				   float vaOverlap_i_i1 = CalculateValueAreaOverlap(profile_i.ValueAreaHigh, profile_i.ValueAreaLow, profile_i1.ValueAreaHigh, profile_i1.ValueAreaLow, TickSize);
				   if (vaOverlap_i_i1 >= MinVAOverlap) { 
					   startBA = true; 
					   initiationReason = "VA Overlap"; 
				   }
			   }
			   
			   if (!startBA) {
				   float rangeDiffPercent = CalculateRangeSimilarityDiff(profile_i, profile_i1, TickSize);
				   float maxAllowedHigh = CalculateMaxAllowedHigh(profile_i.HighestPrice, profile_i.GetRange(), HighLowTolerancePercent, TickSize);
				   float minAllowedLow = CalculateMinAllowedLow(profile_i.LowestPrice, profile_i.GetRange(), HighLowTolerancePercent, TickSize);
				   bool similarRange = CheckRangeSimilarity(rangeDiffPercent, RangeSimilarityPercent);
				   bool controlledHigh = CheckHighPosition(profile_i1.HighestPrice, maxAllowedHigh);
				   bool controlledLow = CheckLowPosition(profile_i1.LowestPrice, minAllowedLow);
				   if (similarRange && controlledHigh && controlledLow) { 
					   startBA = true; 
					   initiationReason = "Geometric Proximity"; 
				   }
			   }

			   if (startBA) {
				   s_BalanceArea currentBA;
				   currentBA.StartProfileChronoIndex = i; 
				   currentBA.EndProfileChronoIndex = i + 1;
				   currentBA.StartDateTime = profile_i.StartDateTime; 
				   currentBA.StartBarIndex = profile_i.BeginIndex;
				   currentBA.EndDateTime = profile_i1.EndDateTime; 
				   currentBA.EndBarIndex = profile_i1.EndIndex;
				   currentBA.IncludedProfileIndices = {i, i+1}; 
				   currentBA.InitiationReason = initiationReason;

				   PriceVolumeMap currentMergedMap;
				   std::vector<std::reference_wrapper<const PriceVolumeMap>> mapsToMerge = { std::cref(profile_i.PriceMap), std::cref(profile_i1.PriceMap) };
				   currentMergedMap = MergeMultipleVolumeProfiles(mapsToMerge);
				   float initialMergedHigh, initialMergedLow; // These will be set by CalculateProfileMetrics
				   CalculateProfileMetrics(currentMergedMap, ValueAreaPercentage, sc, currentBA.POC, currentBA.ValueAreaHigh, currentBA.ValueAreaLow, initialMergedHigh, initialMergedLow, currentBA.TotalVolume);
				   currentBA.HighestPrice = initialMergedHigh; 
				   currentBA.LowestPrice = initialMergedLow;

				   // Additional check for BA validity after merging first two profiles
				   if (currentBA.HighestPrice <= -FLT_MAX || currentBA.LowestPrice >= FLT_MAX || currentBA.HighestPrice < currentBA.LowestPrice) {
						if (DebugBAFormation) {
						   logMsg.Format("DEBUG BA: BA initiated at %d with %d has invalid merged H/L (%.2f/%.2f) or zero range. Discarding.", i, i+1, currentBA.HighestPrice, currentBA.LowestPrice);
						   sc.AddMessageToLog(logMsg,0);
						}
						continue; // Skip this BA if it's invalid from the start
				   }

				   profileUsed[i] = true; 
				   profileUsed[i+1] = true;
				   if (DebugBAFormation) { 
					   logMsg.Format("DEBUG BA: Initiated BA at Profile %d with Profile %d. Reason: '%s'. Initial Range: %.2f-%.2f, VA: %.2f-%.2f, POC: %.2f", i, i + 1, initiationReason.c_str(), currentBA.LowestPrice, currentBA.HighestPrice, currentBA.ValueAreaLow, currentBA.ValueAreaHigh, currentBA.POC); 
					   sc.AddMessageToLog(logMsg, 0); 
				   }

				   // Extension logic
				   for (int k = i + 2; k < numProfilesCollected; ++k) {
					   const s_SessionProfile& profile_k = SessionProfiles[k];
					   if (profile_k.HighestPrice <= -FLT_MAX || profile_k.LowestPrice >= FLT_MAX || profile_k.HighestPrice < profile_k.LowestPrice) {
							if (DebugBAFormation) { 
								logMsg.Format("DEBUG BA: Eval Prof %d for extension - invalid profile data (H:%.2f L:%.2f). Stopping extension.", k, profile_k.HighestPrice, profile_k.LowestPrice); 
								sc.AddMessageToLog(logMsg, 0); 
							}
							break; // Stop extension if current profile is invalid
					   }

					   bool extendBA = false; 
					   std::string extensionReason = "None"; 
					   bool geoHighOK_ext = false; 
					   bool geoLowOK_ext = false;
					   if (DebugBAFormation) { 
						   logMsg.Format("DEBUG BA: Eval Prof %d for extension of BA [%d..%d] (Range: %.2f-%.2f, VA: %.2f-%.2f)", k, currentBA.StartProfileChronoIndex, currentBA.EndProfileChronoIndex, currentBA.LowestPrice, currentBA.HighestPrice, currentBA.ValueAreaLow, currentBA.ValueAreaHigh); 
						   sc.AddMessageToLog(logMsg, 0); 
					   }

					   float overlap_merged_k = CalculateVolumeProfileOverlap(currentMergedMap, profile_k.PriceMap);
					   bool volOverlapPassed = (overlap_merged_k >= MinVolOverlap);
					   if (DebugBAFormation) { 
						   logMsg.Format("  > Vol Overlap Check: Merged BA vs Prof %d = %.1f%%. Threshold = %.1f%%. -> %s", k, overlap_merged_k, MinVolOverlap, (volOverlapPassed ? "PASS" : "FAIL") ); 
						   sc.AddMessageToLog(logMsg, 0); 
					   }
					   if (volOverlapPassed) { 
						   extendBA = true; 
						   extensionReason = "Volume Overlap"; 
					   }

					   if (!extendBA) {
						   float tickTolerance = TickSize / 2.0f;
						   bool highContained = (profile_k.HighestPrice <= currentBA.HighestPrice + tickTolerance);
						   bool lowContained = (profile_k.LowestPrice >= currentBA.LowestPrice - tickTolerance);
						   bool isContained = highContained && lowContained;
						   if (DebugBAFormation) { 
							   logMsg.Format("  > Range Containment Check: Prof %d H=%.2f vs BA H=%.2f(+%.2f)=%s, L=%.2f vs BA L=%.2f(-%.2f)=%s -> %s", k, profile_k.HighestPrice, currentBA.HighestPrice, tickTolerance, (highContained ? "OK" : "X"), profile_k.LowestPrice, currentBA.LowestPrice, tickTolerance, (lowContained ? "OK" : "X"), (isContained ? "PASS" : "FAIL") ); 
							   sc.AddMessageToLog(logMsg, 0); 
						   }
						   if (isContained) { 
							   extendBA = true; 
							   extensionReason = "Range Containment"; 
						   }
					   } else if (DebugBAFormation) { 
						   sc.AddMessageToLog("  > Range Containment Check: Skipped (Vol Overlap Passed)", 0); 
					   }

					   if (!extendBA) {
						   float currentBARange = currentBA.GetRange();
						   float baMaxAllowedHigh = CalculateMaxAllowedHigh(currentBA.HighestPrice, currentBARange, HighLowTolerancePercent, TickSize);
						   float baMinAllowedLow = CalculateMinAllowedLow(currentBA.LowestPrice, currentBARange, HighLowTolerancePercent, TickSize);
						   geoHighOK_ext = CheckHighPosition(profile_k.HighestPrice, baMaxAllowedHigh); 
						   geoLowOK_ext = CheckLowPosition(profile_k.LowestPrice, baMinAllowedLow);
						   bool geometricProximityLiteOK = geoHighOK_ext && geoLowOK_ext;
						   if (DebugBAFormation) { 
							   logMsg.Format("  > Geo Prox Lite Check (Tol=%.1f%%): Prof %d H=%.2f vs MaxAllowH=%.2f (%s), L=%.2f vs MinAllowL=%.2f (%s) -> %s", HighLowTolerancePercent, k, profile_k.HighestPrice, baMaxAllowedHigh, (geoHighOK_ext ? "OK" : "FAIL"), profile_k.LowestPrice, baMinAllowedLow, (geoLowOK_ext ? "OK" : "FAIL"), (geometricProximityLiteOK ? "PASS" : "FAIL") ); 
							   sc.AddMessageToLog(logMsg, 0); 
						   }
						   if (geometricProximityLiteOK) { 
							   extendBA = true; 
							   extensionReason = "Geometric Proximity Lite"; 
						   }
					   } else if (DebugBAFormation) { 
						   sc.AddMessageToLog("  > Geo Prox Lite Check: Skipped (Previous Check Passed)", 0); 
					   }

					   if (!extendBA && (geoHighOK_ext != geoLowOK_ext)) {
						   float closePrice = (profile_k.EndIndex >= 0 && profile_k.EndIndex < sc.ArraySize) ? sc.Close[profile_k.EndIndex] : -FLT_MAX;
						   bool checkPassed = false; 
						   std::string condCloseFailSide = "";
						   if (closePrice > -FLT_MAX && currentBA.LowestPrice < FLT_MAX && currentBA.HighestPrice > -FLT_MAX && currentBA.HighestPrice > currentBA.LowestPrice) { // Ensure BA range is valid
							   if (!geoLowOK_ext && geoHighOK_ext && (closePrice > currentBA.LowestPrice)) { 
								   checkPassed = true; 
								   extensionReason = "Close Above BA Low (Low Fail)"; 
								   condCloseFailSide = "Low"; 
							   } else if (!geoHighOK_ext && geoLowOK_ext && (closePrice < currentBA.HighestPrice)) { 
								   checkPassed = true; 
								   extensionReason = "Close Below BA High (High Fail)"; 
								   condCloseFailSide = "High"; 
							   }
						   }
						   if (DebugBAFormation) { 
							   bool closeInRange = (closePrice > -FLT_MAX && closePrice >= currentBA.LowestPrice && closePrice <= currentBA.HighestPrice); 
							   logMsg.Format("  > Cond. Close Check (Geo %s Fail): Prof %d Close=%.2f. BA Range=[%.2f, %.2f]. Close in Range? %s -> %s", condCloseFailSide.c_str(), k, closePrice, currentBA.LowestPrice, currentBA.HighestPrice, (closeInRange ? "Yes" : "No"), (checkPassed ? "PASS" : "FAIL") ); 
							   sc.AddMessageToLog(logMsg, 0); 
						   }
						   if (checkPassed) { 
							   extendBA = true; 
						   }
					   }

					   if (extendBA) {
						   if (DebugBAFormation) { 
							   logMsg.Format("DEBUG BA: ---> EXTENDED BA [%d..%d] with Profile %d. Reason: '%s'", currentBA.StartProfileChronoIndex, currentBA.EndProfileChronoIndex, k, extensionReason.c_str()); 
							   sc.AddMessageToLog(logMsg, 0); 
						   }
						   currentBA.IncludedProfileIndices.push_back(k); 
						   currentBA.EndProfileChronoIndex = k;
						   currentBA.EndDateTime = profile_k.EndDateTime; 
						   currentBA.EndBarIndex = profile_k.EndIndex;
						   profileUsed[k] = true;
						   mapsToMerge.push_back(std::cref(profile_k.PriceMap));
						   currentMergedMap = MergeMultipleVolumeProfiles(mapsToMerge);
						   float tempPOC, tempVAH, tempVAL, tempVolume, mergedHigh, mergedLow;
						   CalculateProfileMetrics(currentMergedMap, ValueAreaPercentage, sc, tempPOC, tempVAH, tempVAL, mergedHigh, mergedLow, tempVolume);
						   currentBA.POC = tempPOC; 
						   currentBA.ValueAreaHigh = tempVAH; 
						   currentBA.ValueAreaLow = tempVAL; 
						   currentBA.TotalVolume = tempVolume;
						   
						   // Check if the merged BA is still valid after adding profile_k
						   if (mergedHigh <= -FLT_MAX || mergedLow >= FLT_MAX || mergedHigh < mergedLow) {
								if (DebugBAFormation) {
								   logMsg.Format("DEBUG BA: BA extended with %d resulted in invalid merged H/L (%.2f/%.2f). Reverting extension.", k, mergedHigh, mergedLow);
								   sc.AddMessageToLog(logMsg,0);
								}
								profileUsed[k] = false; // Unmark as used as this extension failed
								currentBA.IncludedProfileIndices.pop_back(); // Remove k
								currentBA.EndProfileChronoIndex = (currentBA.IncludedProfileIndices.empty() ? -1 : currentBA.IncludedProfileIndices.back()); // Revert to previous last profile
								if(currentBA.EndProfileChronoIndex != -1) {
								   currentBA.EndDateTime = SessionProfiles[currentBA.EndProfileChronoIndex].EndDateTime;
								   currentBA.EndBarIndex = SessionProfiles[currentBA.EndProfileChronoIndex].EndIndex;
								} else { // Should not happen if BA started with 2 profiles
								   currentBA.EndDateTime = 0; 
								   currentBA.EndBarIndex = -1;
								}
								mapsToMerge.pop_back(); // Remove profile_k's map from merge list
								currentMergedMap = MergeMultipleVolumeProfiles(mapsToMerge); // Re-merge without k
								// Recalculate metrics for the BA without profile_k
								CalculateProfileMetrics(currentMergedMap, ValueAreaPercentage, sc, currentBA.POC, currentBA.ValueAreaHigh, currentBA.ValueAreaLow, currentBA.HighestPrice, currentBA.LowestPrice, currentBA.TotalVolume);
								break; // Stop extending with this invalid profile_k
						   }

						   bool isConditionalClose = (extensionReason == "Close Above BA Low (Low Fail)" || extensionReason == "Close Below BA High (High Fail)");
						   if (isConditionalClose) {
							   float tolerance = TickSize / 2.0f;
							   if (profile_k.HighestPrice > currentBA.HighestPrice + tolerance) {
								   int exactHighProbeBarIndex = -1;
								   if (profile_k.BeginIndex >= 0 && profile_k.EndIndex >= profile_k.BeginIndex) {
									   for (int j_idx = profile_k.BeginIndex; j_idx <= profile_k.EndIndex; ++j_idx) { 
										   if (j_idx < 0 || j_idx >= sc.ArraySize) continue; 
										   if (std::fabs(sc.High[j_idx] - profile_k.HighestPrice) < tolerance) { 
											   exactHighProbeBarIndex = j_idx; 
											   break; 
										   } 
									   }
								   }
								   if (exactHighProbeBarIndex != -1) { 
									   s_ProbeLineDrawingInfo probeInfo = {exactHighProbeBarIndex, profile_k.EndIndex, profile_k.HighestPrice, true}; 
									   pData->ProbeLinesToDraw.push_back(probeInfo); 
									   if (DebugBAFormation) sc.AddMessageToLog("    * Probe Detected (High)", 0); 
								   }
							   }
							   if (profile_k.LowestPrice < currentBA.LowestPrice - tolerance) {
								   int exactLowProbeBarIndex = -1;
								   if (profile_k.BeginIndex >= 0 && profile_k.EndIndex >= profile_k.BeginIndex) {
									   for (int j_idx = profile_k.BeginIndex; j_idx <= profile_k.EndIndex; ++j_idx) { 
										   if (j_idx < 0 || j_idx >= sc.ArraySize) continue; 
										   if (std::fabs(sc.Low[j_idx] - profile_k.LowestPrice) < tolerance) { 
											   exactLowProbeBarIndex = j_idx; 
											   break; 
										   } 
									   }
								   }
								   if (exactLowProbeBarIndex != -1) { 
									   s_ProbeLineDrawingInfo probeInfo = {exactLowProbeBarIndex, profile_k.EndIndex, profile_k.LowestPrice, false}; 
									   pData->ProbeLinesToDraw.push_back(probeInfo); 
									   if (DebugBAFormation) sc.AddMessageToLog("    * Probe Detected (Low)", 0); 
								   }
							   }
						   } else { // Not a conditional close, update BA H/L with merged H/L
							   currentBA.HighestPrice = mergedHigh; 
							   currentBA.LowestPrice = mergedLow;
						   }
					   } else {
						   if (DebugBAFormation) { 
							   logMsg.Format("DEBUG BA: ---X STOPPED Extension of BA [%d..%d] at Profile %d. No criteria met.", currentBA.StartProfileChronoIndex, currentBA.EndProfileChronoIndex, k); 
							   sc.AddMessageToLog(logMsg, 0); 
						   }
						   break;
					   }
				   } // End k loop (extension)

				   bool meetsNormalityCriteria = true;
				   if (FilterByNormality) {
					   s_DistributionStats distStats = CalculateVolumeDistributionStats(currentMergedMap, TickSize);
					   if (!distStats.sufficientData) {
						   meetsNormalityCriteria = false;
						   if (DebugBAFormation) { 
							   logMsg.Format("DEBUG BA: Normality Check for BA [%d..%d]: Insufficient data (Levels w/ Vol: %d). Filter FAILED.", currentBA.StartProfileChronoIndex, currentBA.EndProfileChronoIndex, distStats.numPriceLevelsWithVolume); 
							   sc.AddMessageToLog(logMsg, 0); 
						   }
					   } else {
						   bool skewOK = std::fabs(distStats.skewness) <= MaxAbsSkewness;
						   bool kurtOK = distStats.excessKurtosis >= MinExcessKurtosis && distStats.excessKurtosis <= MaxExcessKurtosis;
						   meetsNormalityCriteria = skewOK && kurtOK;
						   if (DebugBAFormation) { 
							   logMsg.Format("DEBUG BA: Normality Check for BA [%d..%d]: Skew=%.2f (AbsLim=%.2f, OK=%d), Kurt=%.2f (Lims=[%.2f,%.2f], OK=%d). Levels=%d, Mean=%.2f, StdD=%.2f. Overall Pass: %d", currentBA.StartProfileChronoIndex, currentBA.EndProfileChronoIndex, distStats.skewness, MaxAbsSkewness, skewOK, distStats.excessKurtosis, MinExcessKurtosis, MaxExcessKurtosis, kurtOK, distStats.numPriceLevelsWithVolume, distStats.mean, distStats.stdDev, meetsNormalityCriteria); 
							   sc.AddMessageToLog(logMsg, 0); 
						   }
					   }
				   }

				   if (meetsNormalityCriteria) {
					   // Final check for valid BA range before adding
					   if (currentBA.HighestPrice > -FLT_MAX && currentBA.LowestPrice < FLT_MAX && currentBA.HighestPrice >= currentBA.LowestPrice && currentBA.GetRange() >= TickSize / 2.0f) {
						   pData->FinalizedBalanceAreas.push_back(currentBA);
						   if (DebugBAFormation) { 
							   logMsg.Format("DEBUG BA: Finalized BA [%d..%d]. Total Profiles: %d. Range: %.2f-%.2f, VA: %.2f-%.2f, POC: %.2f", currentBA.StartProfileChronoIndex, currentBA.EndProfileChronoIndex, (int)currentBA.IncludedProfileIndices.size(), currentBA.LowestPrice, currentBA.HighestPrice, currentBA.ValueAreaLow, currentBA.ValueAreaHigh, currentBA.POC); 
							   sc.AddMessageToLog(logMsg, 0); 
						   }
					   } else if (DebugBAFormation) {
						   logMsg.Format("DEBUG BA: DISCARDED BA (after extension loop) [%d..%d] due to invalid H/L Range: %.2f-%.2f or too small range.", currentBA.StartProfileChronoIndex, currentBA.EndProfileChronoIndex, currentBA.LowestPrice, currentBA.HighestPrice);
						   sc.AddMessageToLog(logMsg, 0);
					   }
				   } else {
					   if (DebugBAFormation) { 
						   logMsg.Format("DEBUG BA: DISCARDED BA [%d..%d] due to failing normality criteria. Total Profiles: %d", currentBA.StartProfileChronoIndex, currentBA.EndProfileChronoIndex, (int)currentBA.IncludedProfileIndices.size()); 
						   sc.AddMessageToLog(logMsg, 0); 
					   }
				   }
			   } // End if(startBA)
		   } // End i loop (initiation)
// Composite BA Logic (matches original structure)
       if (pData->FinalizedBalanceAreas.size() >= 3) {
           if (DebugCompositeBA) sc.AddMessageToLog("--- Starting Composite BA Detection ---", 0);
           const float compositeOverlapThreshold = 30.0f; 
           const float shiftMagnitudePercent = 20.0f; 
           const int temporalGapLimit = 5;
           std::vector<bool> baAttributed(pData->FinalizedBalanceAreas.size(), false);
           for (size_t j = 0; j <= pData->FinalizedBalanceAreas.size() - 3; ++j) { // Use size_t for loop, ensure comparison is safe
               bool skipped = false; 
               std::string skipReason = "";
               if (baAttributed[j]) { 
                   skipped = true; 
                   skipReason = "BA[" + std::to_string(j) + "] Attributed"; 
               } else if (baAttributed[j+1]) { 
                   skipped = true; 
                   skipReason = "BA[" + std::to_string(j+1) + "] Attributed"; 
               } else if (baAttributed[j+2]) { 
                   skipped = true; 
                   skipReason = "BA[" + std::to_string(j+2) + "] Attributed"; 
               }
               if (skipped) { 
                   if (DebugCompositeBA) { 
                       logMsg.Format("Comp Check BA[%zu..%zu]: Skipped (Reason: %s)", j, j+2, skipReason.c_str()); 
                       sc.AddMessageToLog(logMsg, 0); 
                   } 
                   continue; 
               }

               const s_BalanceArea& ba1 = pData->FinalizedBalanceAreas[j];
               const s_BalanceArea& ba2 = pData->FinalizedBalanceAreas[j+1];
               const s_BalanceArea& ba3 = pData->FinalizedBalanceAreas[j+2];
               float overlap_12 = CalculateRangeOverlapPercent_RelativeToSmaller(ba1, ba2, TickSize); 
               float overlap_13 = CalculateRangeOverlapPercent_RelativeToSmaller(ba1, ba3, TickSize); 
               float overlap_23 = CalculateRangeOverlapPercent_RelativeToSmaller(ba2, ba3, TickSize);
               bool hasOverlap_12 = overlap_12 > 0.0f; 
               bool hasOverlap_13 = overlap_13 > 0.0f; 
               bool hasOverlap_23 = overlap_23 > 0.0f;
               bool meetsThreshold_12 = overlap_12 >= compositeOverlapThreshold; 
               bool meetsThreshold_13 = overlap_13 >= compositeOverlapThreshold; 
               bool meetsThreshold_23 = overlap_23 >= compositeOverlapThreshold;
               int numThresholdMet = (meetsThreshold_12 ? 1 : 0) + (meetsThreshold_13 ? 1 : 0) + (meetsThreshold_23 ? 1 : 0);
               int numAnyOverlap = (hasOverlap_12 ? 1 : 0) + (hasOverlap_13 ? 1 : 0) + (hasOverlap_23 ? 1 : 0);
               std::string overlapType = "No Overlap"; 
               if (numThresholdMet == 3) { 
                   overlapType = "Strong Overlap"; 
               } else if (numAnyOverlap == 3) { 
                   overlapType = "Full Overlap"; 
               } else if (numAnyOverlap == 2) { 
                   overlapType = "Partial Overlap"; 
               } else if (numAnyOverlap == 1) { 
                   overlapType = "1 Overlap"; 
               }
               std::string patternType = "None"; 
               bool is_HLH = false; 
               bool is_LHL = false;
               if (ba1.HighestPrice > -FLT_MAX && ba1.LowestPrice < FLT_MAX && ba2.HighestPrice > -FLT_MAX && ba2.LowestPrice < FLT_MAX && ba3.HighestPrice > -FLT_MAX && ba3.LowestPrice < FLT_MAX) {
                   if ((ba2.HighestPrice < ba1.HighestPrice && ba2.LowestPrice < ba1.LowestPrice) && (ba3.HighestPrice > ba2.HighestPrice && ba3.LowestPrice > ba2.LowestPrice)) { 
                       is_HLH = true; 
                       patternType = "HLH"; 
                   } else if ((ba2.HighestPrice > ba1.HighestPrice && ba2.LowestPrice > ba1.LowestPrice) && (ba3.HighestPrice < ba2.HighestPrice && ba3.LowestPrice < ba2.LowestPrice)) { 
                       is_LHL = true; 
                       patternType = "LHL"; 
                   }
               }
               bool containmentPassed = true; 
               bool shiftMagnitudePassed = true; 
               bool ba1_ba2_GapCheckPassed = true;
               std::string containmentResultStr = ""; 
               std::string shiftResultStr = ""; 
               std::string ba1_ba2_GapResultStr = "";
               if (is_HLH || is_LHL) {
                   containmentPassed = false; 
                   float referenceRange = std::max(ba1.HighestPrice, ba2.HighestPrice) - std::min(ba1.LowestPrice, ba2.LowestPrice); 
                   referenceRange = std::max(referenceRange, TickSize); 
                   float toleranceValue = referenceRange * (RangeContainmentPercent / 100.0f); 
                   float overshootAmount = 0.0f;
                   if (is_HLH) { 
                       float allowedHigh = ba1.HighestPrice + toleranceValue; 
                       if (ba3.HighestPrice <= allowedHigh) containmentPassed = true; 
                       else overshootAmount = ba3.HighestPrice - allowedHigh; 
                   } else { 
                       float allowedLow = ba1.LowestPrice - toleranceValue; 
                       if (ba3.LowestPrice >= allowedLow) containmentPassed = true; 
                       else overshootAmount = allowedLow - ba3.LowestPrice; 
                   }
                   if (containmentPassed) { 
                       containmentResultStr = " (Containment Passed)"; 
                   } else { 
                       float overshootPercent = (referenceRange > TickSize / 2.0f) ? (overshootAmount / referenceRange) * 100.0f : 0.0f; 
                       SCString failDetails; 
                       failDetails.Format(" (Containment Failed: RefR=%.2f, Over=%.2f (%.1f%%))", referenceRange, overshootAmount, overshootPercent); 
                       containmentResultStr = failDetails.GetChars(); 
                   }
                   shiftMagnitudePassed = false; 
                   float ba2_range = std::max(ba2.GetRange(), TickSize); 
                   float shift_threshold_amount = ba2_range * (shiftMagnitudePercent / 100.0f);
                   if (is_HLH) { 
                       if ((ba3.HighestPrice > ba2.HighestPrice + shift_threshold_amount) && (ba3.LowestPrice > ba2.LowestPrice + shift_threshold_amount)) { 
                           shiftMagnitudePassed = true; 
                       } 
                   } else { 
                       if ((ba3.HighestPrice < ba2.HighestPrice - shift_threshold_amount) && (ba3.LowestPrice < ba2.LowestPrice - shift_threshold_amount)) { 
                           shiftMagnitudePassed = true; 
                       } 
                   }
                   if (shiftMagnitudePassed) { 
                       shiftResultStr = " (Shift Passed)"; 
                   } else { 
                       SCString shiftFailDetails; 
                       shiftFailDetails.Format(" (Shift Failed: Req=%.2f)", shift_threshold_amount); 
                       shiftResultStr = shiftFailDetails.GetChars(); 
                   }
                   ba1_ba2_GapCheckPassed = false; 
                   float ba1_range = std::max(ba1.GetRange(), TickSize); // Range of BA1
                   // Check if BA2 is not "too far" from BA1 relative to BA1's range (simplified gap check)
                   if (is_HLH) { 
                       if (ba2.HighestPrice > (ba1.LowestPrice - ba1_range)) ba1_ba2_GapCheckPassed = true;
                   } else { 
                       if (ba2.LowestPrice < (ba1.HighestPrice + ba1_range)) ba1_ba2_GapCheckPassed = true; 
                   }
                   if (ba1_ba2_GapCheckPassed) { 
                       ba1_ba2_GapResultStr = " (Gap12 OK)"; 
                   } else { 
                       ba1_ba2_GapResultStr = " (Gap12 Failed)"; 
                   }
               }
               int temporalGap = CheckTemporalProximity(ba1, ba3, SessionProfiles, pData->FinalizedBalanceAreas, sc); 
               bool temporalPassed = (temporalGap != -1 && temporalGap <= temporalGapLimit);
               std::string temporalGapStdStr = ""; 
               SCString temporalGapSCStr; 
               if (temporalGap != -1) { 
                   temporalGapSCStr.Format(" (TemporalGap=%d)", temporalGap); 
                   temporalGapStdStr = temporalGapSCStr.GetChars(); 
               } else { 
                   temporalGapStdStr = " (TemporalGap Error)"; 
               }
               bool qualifiesAsComposite = false; 
               std::string finalReason = "N/A";
               if (overlapType == "Strong Overlap") { 
                   qualifiesAsComposite = true; 
                   finalReason = "Strong Overlap"; 
               } else if (overlapType == "Full Overlap" || overlapType == "Partial Overlap") { 
                   if (is_HLH || is_LHL) { 
                       if (containmentPassed) { 
                           if (shiftMagnitudePassed) { 
                               if (ba1_ba2_GapCheckPassed) { 
                                   qualifiesAsComposite = true; 
                                   finalReason = patternType + "+Contain+Shift+Gap12"; 
                               } else { 
                                   finalReason = "Gap12 Failed"; 
                               }
                           } else { 
                               finalReason = "Shift Failed"; 
                           }
                       } else { 
                           finalReason = "Containment Failed"; 
                       }
                   } else { 
                       finalReason = "No Pattern"; 
                   } 
               } else if (overlapType == "1 Overlap") { 
                   if (is_HLH || is_LHL) { 
                       if (containmentPassed) { 
                           if (shiftMagnitudePassed) { 
                               if (ba1_ba2_GapCheckPassed) { 
                                   if (temporalPassed) { 
                                       qualifiesAsComposite = true; 
                                       finalReason = patternType + "+Contain+Shift+Gap12+Temporal"; 
                                   } else { 
                                       finalReason = "Temporal Gap Too Large"; 
                                   }
                               } else { 
                                   finalReason = "Gap12 Failed"; 
                               }
                           } else { 
                               finalReason = "Shift Failed"; 
                           }
                       } else { 
                           finalReason = "Containment Failed"; 
                       }
                   } else { 
                       finalReason = "No Pattern"; 
                   } 
               } else { 
                   finalReason = "No Overlap"; 
               }
               if (qualifiesAsComposite) {
                   s_CompositeBalanceArea newComposite; 
                   newComposite.FirstBAIndex = static_cast<int>(j); 
                   newComposite.SecondBAIndex = static_cast<int>(j + 1); 
                   newComposite.ThirdBAIndex = static_cast<int>(j + 2);
                   newComposite.StartDateTime = ba1.StartDateTime; 
                   newComposite.EndDateTime = ba3.EndDateTime; 
                   newComposite.StartBarIndex = ba1.StartBarIndex; 
                   newComposite.EndBarIndex = ba3.EndBarIndex;
                   newComposite.HighestPrice = std::max({ba1.HighestPrice, ba2.HighestPrice, ba3.HighestPrice}); 
                   newComposite.LowestPrice = std::min({ba1.LowestPrice, ba2.LowestPrice, ba3.LowestPrice});
                   if (ba1.LowestPrice >= FLT_MAX || ba2.LowestPrice >= FLT_MAX || ba3.LowestPrice >= FLT_MAX) newComposite.LowestPrice = FLT_MAX;
                   if (ba1.HighestPrice <= -FLT_MAX || ba2.HighestPrice <= -FLT_MAX || ba3.HighestPrice <= -FLT_MAX) newComposite.HighestPrice = -FLT_MAX;
                   newComposite.QualificationReason = finalReason; 
                   pData->CompositeBAs.push_back(newComposite);
                   baAttributed[j] = true; 
                   baAttributed[j+1] = true; 
                   baAttributed[j+2] = true;
               }
               if (DebugCompositeBA) { 
                   SCString detailedChecksStr; 
                   if (is_HLH || is_LHL) { 
                       detailedChecksStr.Format("%s%s%s", containmentResultStr.c_str(), shiftResultStr.c_str(), ba1_ba2_GapResultStr.c_str()); 
                   } 
                   SCString finalStatusStr; 
                   if (qualifiesAsComposite) { 
                       finalStatusStr.Format(" | Result=Qualified (Reason: %s)", finalReason.c_str()); 
                   } else { 
                       finalStatusStr.Format(" | Result=Rejected (Reason: %s)", finalReason.c_str()); 
                   } 
                   logMsg.Format("Comp Check BA[%zu](%d-%d)/BA[%zu](%d-%d)/BA[%zu](%d-%d): Overlap=%s (%.1f,%.1f,%.1f) Pattern=%s%s%s%s", j, ba1.StartProfileChronoIndex, ba1.EndProfileChronoIndex, j + 1, ba2.StartProfileChronoIndex, ba2.EndProfileChronoIndex, j + 2, ba3.StartProfileChronoIndex, ba3.EndProfileChronoIndex, overlapType.c_str(), overlap_12, overlap_13, overlap_23, patternType.c_str(), detailedChecksStr.GetChars(), temporalGapStdStr.c_str(), finalStatusStr.GetChars()); 
                   sc.AddMessageToLog(logMsg, 0); 
               }
           } // End Composite BA loop (j)
           if (DebugCompositeBA) sc.AddMessageToLog("--- Finished Composite BA Detection ---", 0);
       } else if (DebugCompositeBA) { 
           logMsg.Format("Not enough Finalized BAs (%zu) to perform Composite Check.", pData->FinalizedBalanceAreas.size()); 
           sc.AddMessageToLog(logMsg, 0); 
       }

       // Drawing Formation Phase Rectangles and Labels (Only during recalculation)
       int baDrawCount = 0;
       for (const auto& ba : pData->FinalizedBalanceAreas) {
           if (ba.StartBarIndex < 0 || ba.EndBarIndex < ba.StartBarIndex || 
               ba.ValueAreaHigh <= ba.ValueAreaLow || 
               (ba.ValueAreaHigh - ba.ValueAreaLow) < TickSize / 2.0f) continue;
           
           // Only draw formation rectangle if BA is not yet activated
           if (DrawRectangles && !ba.IsActivated) {
               s_UseTool rect;
               rect.Clear();
               rect.ChartNumber = sc.ChartNumber;
               rect.DrawingType = DRAWING_RECTANGLEHIGHLIGHT;
               rect.Color = RectBorderColor;
               rect.SecondaryColor = RectFillColor;
               rect.LineWidth = RectBorderWidth;
               rect.TransparencyLevel = RectTransparency;
               rect.AddMethod = UTAM_ADD_OR_ADJUST;
               rect.BeginIndex = ba.StartBarIndex;
               rect.EndIndex = ba.EndBarIndex;
               rect.BeginValue = ba.ValueAreaLow;
               rect.EndValue = ba.ValueAreaHigh;
               
               // Add embedded label to blue BA rectangle
               if (ShowLabel) {
                   SCString labelText;
                   labelText.Format("BA (%d Pr: %d-%d)", 
                                   (int)ba.IncludedProfileIndices.size(), 
                                   ba.StartProfileChronoIndex, 
                                   ba.EndProfileChronoIndex);
                   rect.Text = labelText;
                   rect.TextAlignment = DT_RIGHT;
                   rect.FontSize = LabelFontSize;
                   rect.TransparentLabelBackground = 1;
               }
               rect.ShowPrice = 1;
               
               if (AllowUserAdjustment) {
                   rect.AddAsUserDrawnDrawing = 1;
                   rect.AllowSaveToChartbook = 0;
               }
               
               int result = sc.UseTool(rect);
               if (result && AllowUserAdjustment) {
                   pData->CreatedBADrawings.push_back(rect.LineNumber);
               }
           }
           
           baDrawCount++;
       }

       // Draw Probe Lines (Only during recalculation)
       if (DrawProbeLines) {
           for (const auto& probeInfo : pData->ProbeLinesToDraw) {
               if (probeInfo.StartBarIndex < 0 || probeInfo.EndBarIndexOfProfile < 0 || 
                   probeInfo.StartBarIndex >= sc.ArraySize) continue;
               
               int finalEndIndex = probeInfo.EndBarIndexOfProfile;
               
               if (ExtendProbeLines) {
                   // Find intersection with future price action
                   int intersectionIndex = -1;
                   for (int searchIdx = probeInfo.EndBarIndexOfProfile + 1; searchIdx < sc.ArraySize; ++searchIdx) {
                       if (searchIdx < 0) continue;
                       float high_s = sc.High[searchIdx];
                       float low_s = sc.Low[searchIdx];
                       float priceTolerance = TickSize / 2.0f;
                       bool intersected = false;
                       
                       if (probeInfo.IsHighProbe) {
                           if (high_s >= probeInfo.Price - priceTolerance) intersected = true;
                       } else {
                           if (low_s <= probeInfo.Price + priceTolerance) intersected = true;
                       }
                       
                       if (intersected) {
                           intersectionIndex = searchIdx;
                           break;
                       }
                   }
                   
                   if (intersectionIndex != -1) finalEndIndex = intersectionIndex;
                   else finalEndIndex = sc.ArraySize - 1;
               }
               
               if (finalEndIndex < probeInfo.StartBarIndex) finalEndIndex = probeInfo.StartBarIndex;
               if (finalEndIndex >= sc.ArraySize) finalEndIndex = sc.ArraySize - 1;
               
               if (probeInfo.StartBarIndex <= finalEndIndex) {
                   s_UseTool probeLine;
                   probeLine.Clear();
                   probeLine.ChartNumber = sc.ChartNumber;
                   probeLine.DrawingType = DRAWING_LINE;
                   probeLine.LineWidth = ProbeLineWidth;
                   probeLine.LineStyle = ProbeLineStyle;
                   probeLine.AddMethod = UTAM_ADD_OR_ADJUST;
                   probeLine.BeginIndex = probeInfo.StartBarIndex;
                   probeLine.EndIndex = finalEndIndex;
                   probeLine.BeginValue = probeInfo.Price;
                   probeLine.EndValue = probeInfo.Price;
                   probeLine.Color = probeInfo.IsHighProbe ? HighProbeColor : LowProbeColor;
                   
                   if (AllowUserAdjustment) {
                       probeLine.AddAsUserDrawnDrawing = 1;
                       probeLine.AllowSaveToChartbook = 0;
                   }
                   
                   int result = sc.UseTool(probeLine);
                   if (result && AllowUserAdjustment) {
                       pData->CreatedProbeDrawings.push_back(probeLine.LineNumber);
                   }
               }
           }
       }

       // Draw Composite BA Rectangles (Only during recalculation)
       if (DrawCompositeRect) {
           for (const auto& compBA : pData->CompositeBAs) {
               if (compBA.StartBarIndex < 0 || compBA.EndBarIndex < compBA.StartBarIndex || 
                   compBA.HighestPrice <= -FLT_MAX || compBA.LowestPrice >= FLT_MAX || 
                   compBA.HighestPrice <= compBA.LowestPrice) continue;
               
               s_UseTool compRect;
               compRect.Clear();
               compRect.ChartNumber = sc.ChartNumber;
               compRect.DrawingType = DRAWING_RECTANGLEHIGHLIGHT;
               compRect.Color = CompRectBorderColor;
               compRect.SecondaryColor = CompRectFillColor;
               compRect.LineWidth = CompRectBorderWidth;
               compRect.TransparencyLevel = CompRectTransparency;
               compRect.AddMethod = UTAM_ADD_OR_ADJUST;
               compRect.BeginIndex = compBA.StartBarIndex;
               compRect.EndIndex = compBA.EndBarIndex;
               compRect.BeginValue = compBA.LowestPrice;
               compRect.EndValue = compBA.HighestPrice;
               
               if (AllowUserAdjustment) {
                   compRect.AddAsUserDrawnDrawing = 1;
                   compRect.AllowSaveToChartbook = 0;
               }
               
               int result = sc.UseTool(compRect);
               if (result && AllowUserAdjustment) {
                   pData->CreatedCompositeDrawings.push_back(compRect.LineNumber);
               }
           }
       }
   } // End if (needRecalculation)

   // ALWAYS check for activations and update extensions (every update, not just recalculation)
   CheckForBAActivation(sc, pData, TickSize);
   UpdateBAExtensions(sc, pData, TickSize, PBALPierceThreshold);

   // Force redraw of active BAs if any changes occurred
   if (DrawActiveBAs) {
       // Delete existing active BA drawings to redraw with updated endpoints
       for (const auto& activeBa : pData->ActiveBalanceAreas) {
           int extLineNum = 50000 + activeBa.StartProfileChronoIndex * 100 + activeBa.EndProfileChronoIndex;
           if (AllowUserAdjustment) {
               sc.DeleteUserDrawnACSDrawing(sc.ChartNumber, extLineNum);
           } else {
               sc.DeleteACSChartDrawing(sc.ChartNumber, TOOL_DELETE_CHARTDRAWING, extLineNum);
           }
       }
       
       // Delete existing PBAL drawings to redraw
       for (const auto& pbal : pData->PBALsToDraw) {
           int pbalLineNum = 60000 + pbal.OriginStartProfileIndex * 100 + pbal.OriginEndProfileIndex + (pbal.IsHigh ? 50 : 0);
           if (AllowUserAdjustment) {
               sc.DeleteUserDrawnACSDrawing(sc.ChartNumber, pbalLineNum);
           } else {
               sc.DeleteACSChartDrawing(sc.ChartNumber, TOOL_DELETE_CHARTDRAWING, pbalLineNum);
           }
       }
   }

    // ALWAYS draw active BA rectangles (extending or cut) - if enabled
    if (DrawActiveBAs) {
        for (const auto& activeBa : pData->ActiveBalanceAreas) {
            if (activeBa.ActivationBarIndex < 0) continue;
        
        s_UseTool activeRect;
        activeRect.Clear();
        activeRect.ChartNumber = sc.ChartNumber;
        
        // Use extending rectangle if still extending, regular rectangle if cut
        if (activeBa.IsExtending && !activeBa.WasCut) {
            activeRect.DrawingType = DRAWING_RECTANGLE_EXT_HIGHLIGHT; // Still extending
        } else {
            activeRect.DrawingType = DRAWING_RECTANGLEHIGHLIGHT;      // Cut to regular rectangle
        }
        
        activeRect.Color = ActiveRectBorderColor;
        activeRect.SecondaryColor = ActiveRectFillColor;
        activeRect.LineWidth = ActiveRectBorderWidth;
        activeRect.TransparencyLevel = ActiveRectTransparency;
        activeRect.AddMethod = UTAM_ADD_OR_ADJUST;
        
        // Rectangle starts at activation point and extends/ends appropriately
        activeRect.BeginIndex = activeBa.ActivationBarIndex;
        activeRect.EndIndex = activeBa.ExtensionEndIndex;
        activeRect.BeginValue = activeBa.ValueAreaLow;
        activeRect.EndValue = activeBa.ValueAreaHigh;
        
        if (AllowUserAdjustment) {
            activeRect.AddAsUserDrawnDrawing = 1;
            activeRect.AllowSaveToChartbook = 0;
        }
        
        // Use unique line number based on original BA indices
        activeRect.LineNumber = 50000 + activeBa.StartProfileChronoIndex * 100 + activeBa.EndProfileChronoIndex;
        
        sc.UseTool(activeRect);
       
		// Add embedded label to the rectangle if enabled
		if (ActiveShowLabel) {
			// Create date string from the first session of the originating BA
			SCString dateStr;
			// using SCDateTime methods
			if (!activeBa.StartDateTime.IsUnset()) {
				int year = activeBa.StartDateTime.GetYear();
				int month = activeBa.StartDateTime.GetMonth();
				int day = activeBa.StartDateTime.GetDay();
				dateStr.Format("%02d-%02d-%02d", month, day, year % 100);
			} else {
				dateStr = "N/A";
			}
			
			// Format volume in millions with 2 decimal places and sessions with D
			float volumeInMillions = activeBa.TotalVolume / 1000000.0f;
			int sessionCount = (int)activeBa.IncludedProfileIndices.size();
			
			// Format label: Date Volume Sessions
			SCString labelText;
			labelText.Format("%s %.2fM %dD", 
							dateStr.GetChars(),
							volumeInMillions,
							sessionCount);
			
			activeRect.Text = labelText;
			activeRect.TextAlignment = DT_RIGHT;
			activeRect.FontSize = ActiveLabelFontSize;
		}
        
        // Add price labels to the rectangle
        activeRect.ShowPrice = 1;
		activeRect.TransparentLabelBackground = 1;
        
        int result = sc.UseTool(activeRect);
   }
   
    // Draw PBAL rays
    if (DrawActiveBAs) {
        for (const auto& pbal : pData->PBALsToDraw) {
            if (pbal.StartBarIndex < 0 || pbal.StartBarIndex >= sc.ArraySize) continue;
            
            s_UseTool pbalRay;
            pbalRay.Clear();
            pbalRay.ChartNumber = sc.ChartNumber;
            pbalRay.DrawingType = DRAWING_HORIZONTAL_RAY;
            pbalRay.Color = pbal.IsHigh ? RGB(255, 255, 0) : RGB(255, 165, 0); // Yellow for high, orange for low
            pbalRay.LineWidth = RectBorderWidth;
            pbalRay.LineStyle = LINESTYLE_SOLID;
            pbalRay.AddMethod = UTAM_ADD_OR_ADJUST;
            
            pbalRay.BeginIndex = pbal.StartBarIndex;
            pbalRay.EndIndex = pbal.EndBarIndex;
            pbalRay.BeginValue = pbal.Price;
            pbalRay.EndValue = pbal.Price;
            
            // Add label and price display
            pbalRay.Text = pbal.OriginLabel;
            pbalRay.TextAlignment = DT_RIGHT;
            pbalRay.ShowPrice = 1;
			pbalRay.TransparentLabelBackground = 1;
            
            if (AllowUserAdjustment) {
                pbalRay.AddAsUserDrawnDrawing = 1;
                pbalRay.AllowSaveToChartbook = 0;
            }
            
            // Use unique line number
            pbalRay.LineNumber = 60000 + pbal.OriginStartProfileIndex * 100 + pbal.OriginEndProfileIndex + (pbal.IsHigh ? 50 : 0);
            
            sc.UseTool(pbalRay);
        }
    }

   } // End DrawActiveBAs condition
} // End of scsf_BalanceAreaDetection