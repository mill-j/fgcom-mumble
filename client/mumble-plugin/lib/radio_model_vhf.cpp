/* 
 * This file is part of the FGCom-mumble distribution (https://github.com/hbeni/fgcom-mumble).
 * Copyright (c) 2020 Benedikt Hallinger
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <iostream> 
#include <cmath>
#include <regex>
#include "radio_model.h"

/**
 * A VHF based radio model for the FGCom-mumble plugin
 *
 * The model implements basic line-of-sight characteristics.
 */
class FGCom_radiowaveModel_VHF : public FGCom_radiowaveModel {
protected:
    /*
    * Calculate the signal quality loss by power/distance model
    * 
    * It is currently modelled very simply (linearly) and NOT REALISTICALLY!
    * Main target now is to get some geographic separation. Main Factor vor VHF is line-of-sight anyways.
    * TODO: Make this more realistic! Depends probably also on antenna used at sender and receiver.
    * TODO: Take terrain effects into account. We could probably use the 3° ASTER/SRTM data for that. This will mute the radio behind mountains :)
    * current formula: (-1/wr*x^2+100)/100, where wr=wattpower*50 and x=slatDistance in km
    * 
    * @param power in Watts
    * @param dist  slant distance in km
    * @return float with the signal quality for given power and distance
    */
    virtual float calcPowerDistance(float power, double slantDist) {
        float wr = power * 50; // gives maximum range in km for the supplied power
        float sq = (-1/wr*pow(slantDist,2)+100)/100;  // gives @10w: 50km=0.95 100km=0.8 150km=0.55 200km=0.2
        return sq;
    }
    
    
public:
        
    std::string getType() {  return "VHF";  }
    
    bool isCompatible(FGCom_radiowaveModel *otherModel) {
        return otherModel->getType() != "STRING";
    }
    
    // Signal depends on VHF characteristics; that is mostly line-of-sight
    fgcom_radiowave_signal getSignal(double lat1, double lon1, float alt1, double lat2, double lon2, float alt2, float power) {
        struct fgcom_radiowave_signal signal;
    
        // get distance to radio horizon (that is the both ranges combined)
        double radiodist = this->getDistToHorizon(alt1) + this->getDistToHorizon(alt2);
        
        // get surface distance
        double dist = this->getSurfaceDistance(lat1, lon1, lat2, lon2);
        
        // get if they can see each other. VHF will have no connection when no line-of-sight is present.
        double heightAboveHorizon = this->heightAboveHorizon(dist, alt1, alt2);
        if (heightAboveHorizon < 0) return signal;  // no, they cant, bail out without signal.

        // get slant distance (in km) so we can calculate signal strenght based on distance
        double slantDist = this->getSlantDistance(dist, heightAboveHorizon-alt1);
        
        // apply power/distance model
        float ss = this->calcPowerDistance(power, slantDist);
        if (ss <= 0.0) return signal; // in case signal strength got neagative, that means we are out of range (too less tx-power)
        
        // when distance is near the radio horizon, we smoothly cut off the signal, so it doesn't drop sharply to 0
        float usedRange = slantDist/radiodist;
        float usedRange_cutoffPct = 0.9; // at which percent of used radio horizon we start to cut off
        if (usedRange > usedRange_cutoffPct) {
            float loss    = (usedRange - usedRange_cutoffPct) * 10; //convert to percent range: 0.9=0%  0.95=0.5(50%)  1.0=1.0(100%)
            //printf("DBG: distance near radio horizon (%.2f/%.2f=%.2f); raw strength=%.2f; loss=%.2f; result=%.2f\n", slantDist, radiodist, usedRange, ss, loss, ss*(1-loss) );
            ss = ss * (1-loss); // apply loss to signal
        }
        
        // prepare return struct
        signal.quality       = ss;
        signal.direction     = this->getDirection(lat1, lon1, lat2, lon2);
        signal.verticalAngle = this->degreeAboveHorizon(dist, alt2-alt1);
        return signal;
    }
    
    
    /* 
    * Convert 25kHz/8.33kHz channel name frequencies to real carrier frequency.
    * 
    * This is done according to the list from https://833radio.com/news/show/7
    * and much appreciated help from Michael "mickybadia" Filhol (ATC-Pie developer, where some of the code here is borrowed)
    * 
    * @param frq cleaned frequency string like "118.02" or "118.025"
    * @return frequency string of corresponding real wave frequency, like "118.0250"
    */
    std::string conv_chan2freq(std::string frq) {
        std::setlocale(LC_NUMERIC,"C"); // decial points always ".", not ","
        
        std::smatch sm;
        if (std::regex_match(frq, sm, std::regex("^\\d+(\\.?)$") )) {
            // we have some MHz frequency like "123". We ensure a frequency with decimals.
            if (sm[1].length() == 0) {
                frq = frq+".00";
            } else {
                frq = frq+"00";
            }
    //         std::cout << "FGCom_radiowaveModel_VHF::conv_chan2freq(): added two decimals: " << frq << std::endl;
        } else if (std::regex_match(frq, std::regex("^\\d+\\.\\d$") )) {
            // we have some MHz frequency like "123.3". We ensure a frequency with decimals.
            frq = frq+"0";
    //         std::cout << "FGCom_radiowaveModel_VHF::conv_chan2freq(): added one decimal: " << frq << std::endl;
        }

        if (std::regex_match(frq, sm, std::regex("^(\\d+)\\.(\\d)(\\d)$") )) {
            // we have a 25kHz shortened channel name like "123.35". Valid endings are 0, 2, 5, 7
    //         std::cout << "FGCom_radiowaveModel_VHF::conv_chan2freq(): 25khz short name detected: " << frq << std::endl;
            if (sm[3] == "2" || sm[3] == "7") {
                // ._2 and ._7 endings are old shortened names for odd 25kHz-step freq's (.02 -> .0250)
                return frq + "50";
            } else if (sm[3] == "0" || sm[3] == "5") {
                // ._0 and ._5 endings are old shortened names for whole 25kHz-step freq's (.05 -> .0500)
                return frq + "00";
            } else {
    //             std::cout << "FGCom_radiowaveModel_VHF::conv_chan2freq(): invalid ending: " << sm[3] << std::endl;
                return frq; // invalid ending, just return the frq
            }
            
        } else if (std::regex_match(frq, sm, std::regex("^(\\d+)\\.(\\d)(\\d)(\\d)$") )) {
            // we have a proper 25kHz channel name (like "118.025") OR an 8.33 channel name (like "118.015")
            
            std::string lastTwo = std::string(sm[3]) + std::string(sm[4]);
            // if the last two digits form a valid 8.33 spacing channel name, we need to convert them to the base frequency
            if (lastTwo == "05" || lastTwo == "10" ||
                lastTwo == "15" || lastTwo == "30" ||
                lastTwo == "35" || lastTwo == "40" ||
                lastTwo == "55" || lastTwo == "60" ||
                lastTwo == "65" || lastTwo == "80" ||
                lastTwo == "85" || lastTwo == "90"    ) {
    //             std::cout << "FGCom_radiowaveModel_VHF::conv_chan2freq(): 8.33khz channel name detected: " << frq << std::endl;
                // valid 8.33 channel name: Expand to corresponding real frequency
            
                // convert first subchannel to its 25kHz base frequency (like "._30" -> "._25")
    //             std::cout << "FGCom_radiowaveModel_VHF::conv_chan2freq():  preswap=" << lastTwo;
                if (lastTwo == "05") lastTwo = "00";
                if (lastTwo == "30") lastTwo = "25";
                if (lastTwo == "55") lastTwo = "50";
                if (lastTwo == "80") lastTwo = "75";
    //             std::cout << "; postswap=" << lastTwo << std::endl;
                std::string tgtfrq = std::string(sm[1]) + "." + std::string(sm[2]) + lastTwo;
    //             std::cout << "tgtfrq=" << tgtfrq << std::endl;
    
                if (lastTwo == "00" || lastTwo == "25" || lastTwo == "50" || lastTwo == "75") {
                    // just map trough the fixed old 25kHz representations
    //                 std::cout << "  mapTrough=" << tgtfrq+"00" << std::endl;
                    return tgtfrq+"00";
                } else {
                    // get the nearest multiple of the spacing
                    float spacing_MHz = .025 / 3;   // 8.33 kHz in mHz = 0.00833333
                    float tgtfrq_f = std::stof(tgtfrq);
                    int ch_833 = round(tgtfrq_f / spacing_MHz);    // get 8.33 channel number; eg round( 118.025 / 0.0083333)
                    float realFrq_f = ch_833 * spacing_MHz; // calculate 8real .33 channel numbers frequency
    //                 printf("  calculated channel#=%i (=%.5f)\n", ch_833, realFrq_f);
                    
                    // convert back to string for return
                    char buffer [50];
                    int len = sprintf (buffer, "%.5f", realFrq_f);   // 5 digits is 10Hz resolution.
                    return std::string(buffer, len);
                }
            
            } else {
    //             std::cout << "FGCom_radiowaveModel_VHF::conv_chan2freq(): 25khz straight channel name detected: " << frq << std::endl;
                return frq + "0"; // 00, 25, 50, 75 are already straight 25kHz frequencies (.025 => .0250)
            }
            
        } else {
            // it was not parseable (unhandled format, note, we also don't need to handle real wave frequencies; the're used as-is)
    //         std::cout << "FGCom_radiowaveModel_VHF::conv_chan2freq(): unhandled : " << frq << std::endl;
            return frq;
        }
        
    }


    // Frequency match is done with a band method, ie. a match is there if the bands overlap
    float getFrqMatch(std::string frq1_real, std::string frq2_real) {
        std::setlocale(LC_NUMERIC,"C"); // decial points always ".", not ","
    
        float filter = 0.0; // no match in case of errors
        
    //     std::cout << "FGCom_radiowaveModel_VHF::getFrqMatch('" << frq1_real.c_str() << "', '" <<frq2_real.c_str() << "')" << std::endl;
    //     std::cout << "FGCom_radiowaveModel_VHF::getFrqMatch() default string match=" << filter << std::endl;
        
        // see if we can it make more precise.
        // that is the case if we have numerical values (after ignoring prefixes).
        try {
            fgcom_radiowave_freqConvRes frq1_p = FGCom_radiowaveModel::splitFreqString(frq1_real);
            fgcom_radiowave_freqConvRes frq2_p = FGCom_radiowaveModel::splitFreqString(frq2_real);
            if (frq1_p.isNumeric && frq2_p.isNumeric) {
                // numeric frequencies
                float frq1_f = std::stof(frq1_p.frequency);
                float frq2_f = std::stof(frq2_p.frequency);
                
                // calculate absolute "off" tuning / tunable window
                // 1.5-500x  => gives >1.0 at 0.001 (1kHz) difference, declining to 0.0. at 0.003 (3 kHz),
                //              yielding a tunable band of 6kHz around the 8.33kHz channel center,
                //              where the range 7.33->9.33 is perfect signal and 50% signal is at about 2kHz off.
                // TODO: Note, i completely made up those numbers. I have no idea of tuning radios. I just wanted to make sure, that 25kHz radios will receive 8.33 channels with overlapping stepsize while keeping the 8.33 channels distinct and with a gap between windows...
                float diff = std::fabs(frq1_f - frq2_f);
    //             std::cout << "DBG CALC:" << std::endl;
    //             std::cout << "   frq1_p=" << frq1_p.frequency << std::endl;
    //             std::cout << "   frq1_f=" << frq1_f << std::endl;
    //             std::cout << "   frq2_p=" << frq2_p.frequency << std::endl;
    //             std::cout << "   frq2_f=" << frq1_f << std::endl;
                filter     = 1.5 - 500 * diff;
    //             std::cout << "   diff=" << diff << "; filter=" << filter << std::endl;
                if (filter > 1.0) filter = 1.0;
                if (filter < 0.0) filter = 0.0;
                
                return filter;
            } else {
                // not numeric: return default
                return filter;
            }
        } catch (const std::exception& e) {
            // fallback in case of errors: return default
            return filter;
        }
    }
    
};