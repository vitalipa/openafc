#include "CsvWriter.h"
#include "UlsFileReader.h"
#include "AntennaModelMap.h"
#include <QDebug>
#include <QStringList>
#include <limits>
#include <math.h>
#include <iostream>

#define VERSION "1.3.0"

bool removeMobile = false;
bool includeUnii8 = false;

const double speedOfLight = 2.99792458e8;
const double unii5StartFreqMHz = 5925.0;
const double unii5StopFreqMHz  = 6425.0;
const double unii7StartFreqMHz = 6525.0;
const double unii7StopFreqMHz  = 6875.0;
const double unii8StartFreqMHz = 6875.0;
const double unii8StopFreqMHz  = 7125.0;

namespace {
QString makeNumber(const double &d) {
  if (std::isnan(d))
    return "";
  else
    return QString::number(d, 'f', 15);
}

QString makeNumber(const int &i) { return QString::number(i); }

QString charString(char c) {
  if (c < 32) {
    return "";
  }
  return QString(c);
}

double emissionDesignatorToBandwidth(const QString &emDesig) {
  QString frqPart = emDesig.left(4);
  double multi;
  QString unitS;

  if (frqPart.contains("H")) {
    multi = 1;
    unitS = "H";
  } else if (frqPart.contains("K")) {
    multi = 1000;
    unitS = "K";
  } else if (frqPart.contains("M")) {
    multi = 1e6;
    unitS = "M";
  } else if (frqPart.contains("G")) {
    multi = 1e9;
    unitS = "G";
  } else {
    return -1;
  }

  QString num = frqPart.replace(unitS, ".");

  double number = num.toDouble() * multi;

  return number / 1e6; // Convert to MHz
}


// Ensures that all the necessary fields are available to determine if this goes in the regular ULS or anomalous file
// returns a blank string if valid, otherwise a string indicating what failed. 
QString hasNecessaryFields(const UlsEmission &e, UlsPath path, UlsLocation rxLoc, UlsLocation txLoc, UlsAntenna rxAnt, UlsAntenna txAnt, UlsHeader txHeader, QList<UlsLocation> prLocList, QList<UlsAntenna> prAntList) {
  QString failReason = "";
  // check lat/lon degree for rx
  if (isnan(rxLoc.latitude) || isnan(rxLoc.longitude)) {
    failReason.append( "Invalid rx lat degree or long degree, "); 
  } 
  // check lat/lon degree for rx
  if (isnan(txLoc.latitude) || isnan(txLoc.longitude)) {
    failReason.append( "Invalid tx lat degree or long degree, "); 
  } 
  // check tx and rx not at same position
  if (    (failReason == "")
       && (fabs(txLoc.longitude - rxLoc.longitude) <=1.0e-5)
       && (fabs(txLoc.latitude  - rxLoc.latitude ) <=1.0e-5) ) {
    failReason.append( "RX and TX at same location, "); 
  } 
  // check rx latitude/longitude direction
  if(rxLoc.latitudeDirection != 'N' && rxLoc.latitudeDirection != 'S') {
    failReason.append( "Invalid rx latitude direction, "); 
  }
  if(rxLoc.longitudeDirection != 'E' && rxLoc.longitudeDirection != 'W') {
    failReason.append( "Invalid rx longitude direction, "); 
  }
  // check tx latitude/longitude direction
  if(txLoc.latitudeDirection != 'N' && txLoc.latitudeDirection != 'S') {
    failReason.append( "Invalid tx latitude direction, "); 
  }
  if(txLoc.longitudeDirection != 'E' && txLoc.longitudeDirection != 'W') {
    failReason.append( "Invalid tx longitude direction, "); 
  }

  // check recievers height to center RAAT
  // if(isnan(rxAnt.heightToCenterRAAT) || rxAnt.heightToCenterRAAT <= 0) {
  //   failReason.append( "Invalid rx height to center RAAT, "); 
  // }
  // if(isnan(txAnt.heightToCenterRAAT) || txAnt.heightToCenterRAAT <= 0) {
  //   failReason.append( "Invalid tx height to center RAAT, "); 
  // }
  // if(txAnt.heightToCenterRAAT < 3) {
  //   failReason.append( "tx height to center RAAT is < 3m, "); 
  // }

  // Fix gain accordig to R2-AIP-05 in fix_param.py
  // check recievers gain 
  // if(isnan(rxAnt.gain) || rxAnt.gain < 0 ) {
  //   failReason.append("Invalid rx gain value, "); 
  // } 
  // if(rxAnt.gain > 80 ) {
  //   failReason.append("rx gain > 80 dBi, "); 
  // }
  // if(rxAnt.gain < 10 ) {
  //   failReason.append("rx gain < 10 dBi, "); 
 //  }
  
  // mobile 
  if ( removeMobile && (txHeader.mobile == 'Y') ) {
    failReason.append("Mobile is Y, ");
  }

  // radio service code 
  if ( removeMobile &&(strcmp(txHeader.radioServiceCode, "TP") == 0) ) {
    failReason.append("Radio service value of TP, ");
  }

  int prIdx;
  for(prIdx=0; prIdx<std::min(prLocList.size(),prAntList.size()); ++prIdx) {
      const UlsLocation &prLoc = prLocList[prIdx];
      const UlsAntenna &prAnt = prAntList[prIdx];

    // check lat/lon degree for pr
    if (isnan(prLoc.latitudeDeg) || isnan(prLoc.longitudeDeg)) {
      failReason.append( "Invalid passive repeater lat degree or long degree, "); 
    } 
    // check pr latitude/longitude direction
    if(prLoc.latitudeDirection != 'N' && prLoc.latitudeDirection != 'S') {
      failReason.append( "Invalid passive repeater latitude direction, "); 
    }
    if(prLoc.longitudeDirection != 'E' && prLoc.longitudeDirection != 'W') {
      failReason.append( "Invalid passive repeater longitude direction, "); 
    }
    // check recievers height to center RAAT
    if(isnan(prAnt.heightToCenterRAAT) || prAnt.heightToCenterRAAT <= 0) {
      failReason.append( "Invalid passive repeater height to center RAAT, "); 
    }
    if(prAnt.heightToCenterRAAT < 3) {
      failReason.append( "Passive repeater height to center RAAT is < 3m, "); 
    }
  }

  return failReason; 
}

bool SegmentCompare(const UlsSegment& segA, const UlsSegment& segB)
{
    return segA.segmentNumber < segB.segmentNumber;
}

QStringList getCSVHeader(int numPR)
{
    QStringList header;
    header << "Callsign";
    header << "Status";
    header << "Radio Service";
    header << "Entity Name";
    header << "FRN";
    header << "Grant";
    header << "Expiration";
    header << "Effective";
    header << "Address";
    header << "City";
    header << "County";
    header << "State";
    header << "Common Carrier";
    header << "Non Common Carrier";
    header << "Private Comm";
    header << "Fixed";
    header << "Mobile";
    header << "Radiolocation";
    header << "Satellite";
    header << "Developmental or STA or Demo";
    header << "Interconnected";
    header << "Path Number";
    header << "Tx Location Number";
    header << "Tx Antenna Number";
    header << "Rx Callsign";
    header << "Rx Location Number";
    header << "Rx Antenna Number";
    header << "Frequency Number";
    header << "1st Segment Length (km)";
    header << "Center Frequency (MHz)";
    header << "Bandwidth (MHz)";
    header << "Lower Band (MHz)";
    header << "Upper Band (MHz)";
    header << "Tolerance (%)";
    header << "Tx EIRP (dBm)";
    header << "Auto Tx Pwr Control";
    header << "Emissions Designator";
    header << "Digital Mod Rate";
    header << "Digital Mod Type";
    header << "Tx Manufacturer";
    header << "Tx Model";
    header << "Tx Location Name";
    header << "Tx Lat Coords";
    header << "Tx Long Coords";
    header << "Tx Ground Elevation (m)";
    header << "Tx Polarization";
    header << "Tx Azimuth Angle (deg)";
    header << "Tx Elevation Angle (deg)";
    header << "Tx Ant Manufacturer";
    header << "Tx Ant Model";
    header << "Tx Ant Model Name Matched";
    header << "Tx Ant Category";
    header << "Tx Ant Diameter (m)";
    header << "Tx Ant Midband Gain (dB)";
    header << "Tx Height to Center RAAT (m)";
    header << "Tx Beamwidth";
    header << "Tx Gain ULS (dBi)";
    header << "Rx Location Name";
    header << "Rx Lat Coords";
    header << "Rx Long Coords";
    header << "Rx Ground Elevation (m)";
    header << "Rx Manufacturer";
    header << "Rx Model";
    header << "Rx Ant Manufacturer";
    header << "Rx Ant Model";
    header << "Rx Ant Model Name Matched";
    header << "Rx Ant Category";
    header << "Rx Ant Diameter (m)";
    header << "Rx Ant Midband Gain (dB)";
    header << "Rx Line Loss (dB)";
    header << "Rx Height to Center RAAT (m)";
    header << "Rx Gain ULS (dBi)";
    header << "Rx Diversity Height (m)";
    header << "Rx Diversity Gain (dBi)";
    header << "Num Passive Repeater";
    for(int prIdx=1; prIdx<=numPR; ++prIdx) {
        header << "Passive Repeater " + QString::number(prIdx) + " Location Name";
        header << "Passive Repeater " + QString::number(prIdx) + " Lat Coords";
        header << "Passive Repeater " + QString::number(prIdx) + " Long Coords";
        header << "Passive Repeater " + QString::number(prIdx) + " Ground Elevation (m)";
        header << "Passive Repeater " + QString::number(prIdx) + " Polarization";
        header << "Passive Repeater " + QString::number(prIdx) + " Azimuth Angle (deg)";
        header << "Passive Repeater " + QString::number(prIdx) + " Elevation Angle (deg)";
        header << "Passive Repeater " + QString::number(prIdx) + " Ant Manufacturer";
        header << "Passive Repeater " + QString::number(prIdx) + " Ant Model";
        header << "Passive Repeater " + QString::number(prIdx) + " Ant Model Name Matched";
        header << "Passive Repeater " + QString::number(prIdx) + " Ant Type";
        header << "Passive Repeater " + QString::number(prIdx) + " Ant Category";
        header << "Passive Repeater " + QString::number(prIdx) + " ULS Back-to-Back Gain Tx (dBi)";
        header << "Passive Repeater " + QString::number(prIdx) + " ULS Back-to-Back Gain Rx (dBi)";
        header << "Passive Repeater " + QString::number(prIdx) + " ULS Reflector Height (m)";
        header << "Passive Repeater " + QString::number(prIdx) + " ULS Reflector Width (m)";
        header << "Passive Repeater " + QString::number(prIdx) + " Ant Model Diameter (m)";
        header << "Passive Repeater " + QString::number(prIdx) + " Ant Model Midband Gain (dB)";
        header << "Passive Repeater " + QString::number(prIdx) + " Ant Model Reflector Height (m)";
        header << "Passive Repeater " + QString::number(prIdx) + " Ant Model Reflector Width (m)";
        header << "Passive Repeater " + QString::number(prIdx) + " Line Loss (dB)";
        header << "Passive Repeater " + QString::number(prIdx) + " Height to Center RAAT (m)";
        header << "Passive Repeater " + QString::number(prIdx) + " Beamwidth";
        header << "Segment " + QString::number(prIdx+1) + " Length (Km)";
    }

    return header;
}

/******************************************************************************************/
/**** computeSpectralOverlap                                                           ****/
/******************************************************************************************/
double computeSpectralOverlap(double sigStartFreq, double sigStopFreq, double rxStartFreq, double rxStopFreq)
{
    double overlap;

    if ((sigStopFreq <= rxStartFreq) || (sigStartFreq >= rxStopFreq)) {
        overlap = 0.0;
    } else {
        double f1 = (sigStartFreq < rxStartFreq ? rxStartFreq : sigStartFreq);
        double f2 = (sigStopFreq  > rxStopFreq ? rxStopFreq : sigStopFreq);
        overlap = (f2-f1)/(sigStopFreq - sigStartFreq);
    }

    return(overlap);
}
/******************************************************************************************/


}; // namespace

void testAntennaModelMap(AntennaModelMapClass &antennaModelMap, std::string inputFile, std::string outputFile);

int main(int argc, char **argv)
{
  setvbuf(stdout, NULL, _IONBF, 0);

  if (strcmp(argv[1], "--version") == 0) {
    printf("Coalition ULS Processing Tool Version %s\n", VERSION);
    printf("Copyright 2019 (C) RKF Engineering Solutions\n");
    printf("Compatible with ULS Database Version 4\n");
    printf("Spec: "
           "https://www.fcc.gov/sites/default/files/"
           "public_access_database_definitions_v4.pdf\n");
    return 0;
  }
  printf("Coalition ULS Processing Tool Version %s\n", VERSION);
  printf("Copyright 2019 (C) RKF Engineering Solutions\n");
  if (argc != 6) {
    fprintf(stderr, "Syntax: %s [ULS file.csv] [Output File.csv] [AntModelListFile.csv] [AntModelMapFile.csv] [mode]\n", argv[0]);
    return -1;
  }

    char *tstr;

    time_t t1 = time(NULL);
    tstr = strdup(ctime(&t1));
    strtok(tstr, "\n");
    std::cout << tstr << " : Begin processing." << std::endl;
    free(tstr);

    std::string inputFile = argv[1];
    std::string outputFile = argv[2];
    std::string antModelListFile = argv[3];
    std::string antModelMapFile = argv[4];
    std::string mode = argv[5];

    FILE *fwarn;
    std::string warningFile = "warning_uls.txt";
    if ( !(fwarn = fopen(warningFile.c_str(), "wb")) ) {
        std::cout << std::string("WARNING: Unable to open warningFile \"") + warningFile + std::string("\"\n");
    }

	AntennaModelMapClass antennaModelMap(antModelListFile, antModelMapFile);

    if (mode == "test_antenna_model_map") {
        testAntennaModelMap(antennaModelMap, inputFile, outputFile);
        return 0;
    } else if (mode == "proc_uls") {
        // Do nothing
    } else if (mode == "proc_uls_include_unii8") {
        includeUnii8 = true;
    } else {
        fprintf(stderr, "ERROR: Invalid mode: %s\n", mode.c_str());
        return -1;
    }

    int numAntMatch = 0;
    int numAntUnmatch = 0;

  UlsFileReader r(inputFile.c_str(), fwarn);

    int numMissingRxAntHeight = 0;
    int numMissingTxAntHeight = 0;
    int maxNumSegment = 0;
    std::string maxNumSegmentCallsign = "";

    foreach (const UlsFrequency &freq, r.frequencies()) {

        UlsPath path;
        bool pathFound = false;

        foreach (const UlsPath &p, r.pathsMap(freq.callsign)) {
            if (strcmp(p.callsign, freq.callsign) == 0) {
                if (freq.locationNumber == p.txLocationNumber &&
                    freq.antennaNumber == p.txAntennaNumber) {
                    path = p;
                    pathFound = true;
                    break;
                }
            }
        }

        if (pathFound == false) {
            continue;
        }

        /// Find the emissions information.
        UlsEmission txEm;
        bool txEmFound = false;
        bool hasMultipleTxEm = false;
        QList<UlsEmission> allTxEm;
        foreach (const UlsEmission &e, r.emissionsMap(freq.callsign)) {
            if (strcmp(e.callsign, freq.callsign) == 0 &&
                e.locationId == freq.locationNumber &&
                e.antennaId == freq.antennaNumber &&
                e.frequencyId == freq.frequencyNumber) {
                allTxEm << e;
            }
        }

        /// Find the header.
        UlsHeader txHeader;
        bool txHeaderFound = false;
        foreach (const UlsHeader &h, r.headersMap(path.callsign)) {
            if (strcmp(h.callsign, path.callsign) == 0) {
                txHeader = h;
                txHeaderFound = true;
                break;
            }
        }

        if (!txHeaderFound) {
            continue;
        } else if (txHeader.licenseStatus != 'A' && txHeader.licenseStatus != 'L') {
            continue;
        }

        // std::cout << freq.callsign << ": " << allTxEm.size() << " emissions" << std::endl;
        foreach (const UlsEmission &e, allTxEm) {
            bool invalidFlag = false;
            double bwMhz = std::numeric_limits<double>::quiet_NaN();
            double lowFreq, highFreq;
            bwMhz = emissionDesignatorToBandwidth(e.desig);
            if (bwMhz == -1) {
                invalidFlag = true;
            }
            if (bwMhz == 0) {
                invalidFlag = true;
            }

            if (!invalidFlag) {
                if (freq.frequencyUpperBand > freq.frequencyAssigned) {
                    lowFreq  = freq.frequencyAssigned;  // Lower Band (MHz)
                    highFreq = freq.frequencyUpperBand; // Upper Band (MHz)
                    // Here Frequency Assigned should be low + high / 2
                } else {
                    lowFreq  = freq.frequencyAssigned - bwMhz / 2.0; // Lower Band (MHz)
                    highFreq = freq.frequencyAssigned + bwMhz / 2.0; // Upper Band (MHz)
                }
                // skip if no overlap UNII5 and 7
                bool overlapUnii5 = (highFreq > unii5StartFreqMHz) && (lowFreq < unii5StopFreqMHz);
                bool overlapUnii7 = (highFreq > unii7StartFreqMHz) && (lowFreq < unii7StopFreqMHz);
                bool overlapUnii8 = (highFreq > unii8StartFreqMHz) && (lowFreq < unii8StopFreqMHz);

                if (!(overlapUnii5 || overlapUnii7 || (includeUnii8 && overlapUnii8))) {
                    invalidFlag = true;
                }
            } 
            if (!invalidFlag) {
                foreach (const UlsSegment &segment, r.segmentsMap(freq.callsign)) {
                    int segmentNumber = segment.segmentNumber;
                    if (segmentNumber > maxNumSegment) {
                        maxNumSegment = segmentNumber;
                        maxNumSegmentCallsign = std::string(segment.callsign);
                    }
                }
            }
        }
    }

    int maxNumPassiveRepeater = maxNumSegment - 1;

  qDebug() << "DATA statistics:";
  qDebug() << "paths" << r.paths().count();
  qDebug() << "emissions" << r.emissions().count();
  qDebug() << "antennas" << r.antennas().count();
  qDebug() << "frequencies" << r.frequencies().count();
  qDebug() << "locations" << r.locations().count();
  qDebug() << "headers" << r.headers().count();
  qDebug() << "market freqs" << r.marketFrequencies().count();
  qDebug() << "entities" << r.entities().count();
  qDebug() << "control points" << r.controlPoints().count();
  qDebug() << "segments" << r.segments().count();
  qDebug() << "maxNumPassiveRepeater" << maxNumPassiveRepeater << " callsign: " << QString::fromStdString(maxNumSegmentCallsign);

  int prIdx;

    CsvWriter wt(outputFile.c_str());
    {
        QStringList header = getCSVHeader(maxNumPassiveRepeater);
        wt.writeRow(header);
    }

    CsvWriter anomalous("anomalous_uls.csv");
    {
        QStringList header = getCSVHeader(maxNumPassiveRepeater);
        header << "Fixed";
        header << "Anomalous Reason";
        anomalous.writeRow(header);
    }

    qDebug() << "--- Beginning path processing";

    int cnt = 0;
    int numRecs = 0;

  foreach (const UlsFrequency &freq, r.frequencies()) {
    // qDebug() << "processing frequency " << cnt << "/" << r.frequencies().count()
            //  << " callsign " << freq.callsign;
    bool pathFound = false;
    bool hasRepeater = false;
    QString anomalousReason = ""; 
    QString fixedReason = ""; 

    QList<UlsPath> pathList;

    foreach (const UlsPath &p, r.pathsMap(freq.callsign)) {
        if (strcmp(p.callsign, freq.callsign) == 0) {
            if (    (freq.locationNumber == p.txLocationNumber)
                 && (freq.antennaNumber == p.txAntennaNumber) ) {
                pathList << p;
            }
        }
    }

    if ((pathList.size() == 0) && (fwarn)) {
        fprintf(fwarn, "CALLSIGN: %s, Unable to find path matching TX_LOCATION_NUM = %d TX_ANTENNA_NUM = %d\n",
            freq.callsign, freq.locationNumber, freq.antennaNumber);
    }

    foreach (const UlsPath &path, pathList) {
        if (path.passiveReceiver == 'Y') {
            hasRepeater = true;
        }

        cnt++;

        /// Find the associated transmit location.
        UlsLocation txLoc;
        bool locFound = false;
        foreach (const UlsLocation &loc, r.locationsMap(path.callsign)) {
            if (strcmp(loc.callsign, path.callsign) == 0) {
                if (path.txLocationNumber == loc.locationNumber) {
                    txLoc = loc;
                    locFound = true;
                    break;
                }
            }
        }

        if (locFound == false) {
            if (fwarn) {
                fprintf(fwarn, "CALLSIGN: %s, Unable to find txLoc matching LOCATION_NUM = %d\n",
                    freq.callsign, path.txLocationNumber);
            }
            continue;
        }

        /// Find the associated transmit antenna.
        UlsAntenna txAnt;
        bool txAntFound = false;
        foreach (const UlsAntenna &ant, r.antennasMap(path.callsign)) {
            if (strcmp(ant.callsign, path.callsign) == 0) {
                // Implicitly matches with an Antenna record with locationClass 'T'
                if (    (ant.locationNumber == txLoc.locationNumber)
                     && (ant.antennaNumber == path.txAntennaNumber)
                     && (ant.pathNumber == path.pathNumber)
                   ) {
                    txAnt = ant;
                    txAntFound = true;
                    break;
                }
            }
        }

        if (txAntFound == false) {
            if (fwarn) {
                fprintf(fwarn, "CALLSIGN: %s, Unable to find txAnt matching LOCATION_NUM = %d ANTENNA_NUM = %d PATH_NUM = %d\n",
                    freq.callsign, txLoc.locationNumber, path.txAntennaNumber, path.pathNumber);
            }
            continue;
        }

        /// Find the associated frequency.
        const UlsFrequency &txFreq = freq;

        /// Find the RX location.
        UlsLocation rxLoc;
        bool rxLocFound = false;
        foreach (const UlsLocation &loc, r.locationsMap(path.callsign)) {
            if (strcmp(loc.callsign, path.callsign) == 0) {
                if (loc.locationNumber == path.rxLocationNumber) {
                    rxLoc = loc;
                    rxLocFound = true;
                    break;
                }
            }
        }

        if (rxLocFound == false) {
            if (fwarn) {
                fprintf(fwarn, "CALLSIGN: %s, Unable to find rxLoc matching LOCATION_NUM = %d\n",
                    freq.callsign, path.rxLocationNumber);
            }
            continue;
        }

        /// Find the RX antenna.
        UlsAntenna rxAnt;
        bool rxAntFound = false;
        foreach (const UlsAntenna &ant, r.antennasMap(path.callsign)) {
            if (strcmp(ant.callsign, path.callsign) == 0) {
                // Implicitly matches with an Antenna record with locationClass 'R'
                if (    (ant.locationNumber == rxLoc.locationNumber)
                     && (ant.antennaNumber == path.rxAntennaNumber)
                     && (ant.pathNumber == path.pathNumber)
                   ) {
                    rxAnt = ant;
                    rxAntFound = true;
                    break;
                }
            }
        }

        if (rxAntFound == false) {
            if (fwarn) {
                fprintf(fwarn, "CALLSIGN: %s, Unable to find rxAnt matching LOCATION_NUM = %d ANTENNA_NUM = %d PATH_NUM = %d\n",
                    freq.callsign, rxLoc.locationNumber, path.rxAntennaNumber, path.pathNumber);
            }
            continue;
        }

        // Only assigned if hasRepeater is True
        QList<UlsLocation> prLocList;
        QList<UlsAntenna> prAntList;

        /// Create list of segments in link.
        QList<UlsSegment> segList;
        foreach (const UlsSegment &s, r.segmentsMap(path.callsign)) {
            if (s.pathNumber == path.pathNumber) {
                segList << s;
            }
        }
        qSort(segList.begin(), segList.end(), SegmentCompare);

        int prevSegRxLocationId = -1;
        for(int segIdx=0; segIdx<segList.size(); ++segIdx) {
            const UlsSegment &s = segList[segIdx];
            if (s.segmentNumber != segIdx+1) {
                anomalousReason.append("Segments missing, ");
                qDebug() << "callsign " << path.callsign << " path " << path.pathNumber << " has missing segments.";
                break;
            }
            if (segIdx == 0) {
                if (s.txLocationId != txLoc.locationNumber) {
                    anomalousReason.append("First segment not at TX, ");
                    qDebug() << "callsign " << path.callsign << " path " << path.pathNumber << " first segment not at TX.";
                    break;
                }
            }
            if (segIdx == segList.size()-1) {
                if (s.rxLocationId != rxLoc.locationNumber) {
                    anomalousReason.append("Last segment not at RX, ");
                    qDebug() << "callsign " << path.callsign << " path " << path.pathNumber << " last segment not at RX.";
                    break;
                }
            }
            if (segIdx) {
                if (s.txLocationId != prevSegRxLocationId) {
                    anomalousReason.append("Segments do not form a path, ");
                    qDebug() << "callsign " << path.callsign << " path " << path.pathNumber << " segments do not form a path.";
                    break;
                }
                bool found;
                found = false;
                foreach (const UlsLocation &loc, r.locationsMap(path.callsign)) {
                    if (loc.locationNumber == s.txLocationId) {
                        prLocList << loc;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    anomalousReason.append("Segment location not found, ");
                    qDebug() << "callsign " << path.callsign << " path " << path.pathNumber << " segment location not found.";
                    break;
                }
                found = false;
                foreach (const UlsAntenna &ant, r.antennasMap(path.callsign)) {
                    if (    (ant.antennaType == 'P')
                         && (ant.locationNumber == prLocList.last().locationNumber)
                         && (ant.pathNumber == path.pathNumber) ) {
                        prAntList << ant;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    anomalousReason.append("Segment antenna not found, ");
                    qDebug() << "callsign " << path.callsign << " path " << path.pathNumber << " segment antenna not found.";
                    break;
                }
            }
            prevSegRxLocationId = s.rxLocationId;
        }

        UlsSegment txSeg;
        bool txSegFound = false;
        foreach (const UlsSegment &s, r.segmentsMap(path.callsign)) {
            if (strcmp(s.callsign, path.callsign) == 0) {
                if (s.pathNumber == path.pathNumber) {
                    if (s.segmentNumber < 2) {
                        txSeg = s;
                        txSegFound = true;
                        break;
                    }
                }
            }
        }

        /// Find the emissions information.
        UlsEmission txEm;
        bool txEmFound = false;
        bool hasMultipleTxEm = false;
        QList<UlsEmission> allTxEm;
        foreach (const UlsEmission &e, r.emissionsMap(path.callsign)) {
            if (strcmp(e.callsign, path.callsign) == 0) {
                if (e.locationId == txLoc.locationNumber &&
                    e.antennaId == txAnt.antennaNumber &&
                    e.frequencyId == txFreq.frequencyNumber) {
                if (txEmFound) {
                    hasMultipleTxEm = true;
                    allTxEm << e;
                    continue;
                } else {
                    allTxEm << e;
                    txEm = e;
                    txEmFound = true;
                }
                // break;
                }
            }
        }
        if (!txEmFound)
          allTxEm << txEm; // Make sure at least one emission.

        // if (hasMultipleTxEm) {
        //   qDebug() << "callsign " << path.callsign
        //            << " has multiple emission designators.";
        // }

        /// Find the header.
        UlsHeader txHeader;
        bool txHeaderFound = false;
        foreach (const UlsHeader &h, r.headersMap(path.callsign)) {
            if (strcmp(h.callsign, path.callsign) == 0) {
                txHeader = h;
                txHeaderFound = true;
                break;
            }
        }

        if (!txHeaderFound) {
            // qDebug() << "Unable to locate header data for" << path.callsign << "path"
            //  << path.pathNumber;
            continue;
        } else if (txHeader.licenseStatus != 'A' && txHeader.licenseStatus != 'L') {
            // qDebug() << "Skipping non-Active tx for" << path.callsign << "path" << path.pathNumber;
            continue;
        }

        /// Find the entity
        UlsEntity txEntity;
        bool txEntityFound = false;
        foreach (const UlsEntity &e, r.entitiesMap(path.callsign)) {
            if (strcmp(e.callsign, path.callsign) == 0) {
                txEntity = e;
                txEntityFound = true;
                break;
            }
        }

        if (!txEntityFound) {
            // qDebug() << "Unable to locate entity data for" << path.callsign << "path"
            //          << path.pathNumber;
            continue;
        }

        /// Find the control point.
        UlsControlPoint txControlPoint;
        bool txControlPointFound = false;
        foreach (const UlsControlPoint &ucp, r.controlPointsMap(path.callsign)) {
            if (strcmp(ucp.callsign, path.callsign) == 0) {
                txControlPoint = ucp;
                txControlPointFound = true;
                break;
            }
        }

    /// Build the actual output.
    foreach (const UlsEmission &e, allTxEm) {
      double bwMhz = std::numeric_limits<double>::quiet_NaN();
      double lowFreq, highFreq;
      if (txEmFound) {
        bwMhz = emissionDesignatorToBandwidth(e.desig);
        if (bwMhz == -1) {
          anomalousReason.append("Emission designator contains no or invalid order of magnitude, ");
        }
        if (bwMhz == 0) {
          anomalousReason.append("Emission designator contains no or invalid order of magnitude, ");
        }
      }
      if (txFreq.frequencyUpperBand > txFreq.frequencyAssigned) {
        lowFreq = txFreq.frequencyAssigned;  // Lower Band (MHz)
        highFreq = txFreq.frequencyUpperBand; // Upper Band (MHz)
        // Here Frequency Assigned should be low + high / 2
      } else {
        lowFreq = txFreq.frequencyAssigned -
                          bwMhz / 2.0; // Lower Band (MHz)
        highFreq = txFreq.frequencyAssigned +
                          bwMhz / 2.0; // Upper Band (MHz)
      }

      AntennaModelClass *rxAntModel = antennaModelMap.find(std::string(rxAnt.antennaModel));

      AntennaModelClass::CategoryEnum rxAntennaCategory;
      double rxAntennaDiameter;
      double rxAntennaMidbandGain;
      std::string rxAntennaModelName;
      if (rxAntModel) {
          numAntMatch++;
          rxAntennaModelName = rxAntModel->name;
          rxAntennaCategory = rxAntModel->category;
          rxAntennaDiameter = rxAntModel->diameterM;
          rxAntennaMidbandGain = rxAntModel->midbandGain;
      } else {
          numAntUnmatch++;
          rxAntennaModelName = "";
          rxAntennaCategory = AntennaModelClass::UnknownCategory;
          rxAntennaDiameter = -1.0;
          rxAntennaMidbandGain = std::numeric_limits<double>::quiet_NaN();
          fixedReason.append("Rx Antenna Model Unmatched");
      }

      AntennaModelClass *txAntModel = antennaModelMap.find(std::string(txAnt.antennaModel));

      AntennaModelClass::CategoryEnum txAntennaCategory;
      double txAntennaDiameter;
      double txAntennaMidbandGain;
      std::string txAntennaModelName;
      if (txAntModel) {
          numAntMatch++;
          txAntennaModelName = txAntModel->name;
          txAntennaCategory = txAntModel->category;
          txAntennaDiameter = txAntModel->diameterM;
          txAntennaMidbandGain = txAntModel->midbandGain;
      } else {
          numAntUnmatch++;
          txAntennaModelName = "";
          txAntennaCategory = AntennaModelClass::UnknownCategory;
          txAntennaDiameter = -1.0;
          txAntennaMidbandGain = std::numeric_limits<double>::quiet_NaN();
          fixedReason.append("Tx Antenna Model Unmatched");
      }

      if(isnan(lowFreq) || isnan(highFreq)) {
          anomalousReason.append("NaN frequency value, ");
      } else {
          bool overlapUnii5 = (highFreq > unii5StartFreqMHz) && (lowFreq < unii5StopFreqMHz);
          bool overlapUnii7 = (highFreq > unii7StartFreqMHz) && (lowFreq < unii7StopFreqMHz);
          bool overlapUnii8 = (highFreq > unii8StartFreqMHz) && (lowFreq < unii8StopFreqMHz);

          if (!(overlapUnii5 || overlapUnii7 || (includeUnii8 && overlapUnii8))) {
              continue;
          } else if (overlapUnii5 && overlapUnii7) {
              anomalousReason.append("Band overlaps both Unii5 and Unii7, ");
          }
      }

      if (std::isnan(rxAnt.heightToCenterRAAT)) {
          rxAnt.heightToCenterRAAT = -1.0;
          numMissingRxAntHeight++;
      } else if (rxAnt.heightToCenterRAAT < 1.5) {
          rxAnt.heightToCenterRAAT = 1.5;
      }

      if (std::isnan(txAnt.heightToCenterRAAT)) {
          txAnt.heightToCenterRAAT = -1.0;
          numMissingTxAntHeight++;
      } else if (txAnt.heightToCenterRAAT < 1.5) {
          txAnt.heightToCenterRAAT = 1.5;
      }

      // now that we have everything, ensure that inputs have what we need
      anomalousReason.append(hasNecessaryFields(e, path, rxLoc, txLoc, rxAnt, txAnt, txHeader, prLocList, prAntList));

      QStringList row;
      row << path.callsign;                   // Callsign
      row << QString(txHeader.licenseStatus); // Status
      row << txHeader.radioServiceCode;       // Radio Service
      row << txEntity.entityName;             // Entity Name
      row << txEntity.frn;                    // FRN: Fcc Registration Number
      row << txHeader.grantDate;              // Grant
      row << txHeader.expiredDate;            // Expiration
      row << txHeader.effectiveDate;          // Effective
      if (txControlPointFound) {
        row << txControlPoint.controlPointAddress; // Address
        row << txControlPoint.controlPointCity;    // City
        row << txControlPoint.controlPointCounty;  // County
        row << txControlPoint.controlPointState;   // State
      } else {
        row << ""
            << ""
            << ""
            << "";
      }
      row << charString(txHeader.commonCarrier);    // Common Carrier
      row << charString(txHeader.nonCommonCarrier); // Non Common Carrier
      row << charString(txHeader.privateCarrier);   // Private Comm
      row << charString(txHeader.fixed);            // Fixed
      row << charString(txHeader.mobile);           // Mobile
      row << charString(txHeader.radiolocation);    // Radiolocation
      row << charString(txHeader.satellite);        // Satellite
      row << charString(txHeader.developmental); // Developmental or STA or Demo
      row << charString(txHeader.interconnected); // Interconnected
      row << makeNumber(path.pathNumber);         // Path Number
      row << makeNumber(path.txLocationNumber);   // Tx Location Number
      row << makeNumber(path.txAntennaNumber);    // Tx Antenna Number
      row << path.rxCallsign;                     // Rx Callsign
      row << makeNumber(path.rxLocationNumber);   // Rx Location Number
      row << makeNumber(path.rxAntennaNumber);    // Rx Antenna Number
      row << makeNumber(txFreq.frequencyNumber);  // Frequency Number
      if (txSegFound) {
        row << makeNumber(txSeg.segmentLength); // 1st Segment Length (km)
      } else {
        row << "";
      }

      {

        QString _freq;
        // If this is true, frequencyAssigned IS the lower band
        if (txFreq.frequencyUpperBand > txFreq.frequencyAssigned) {
          _freq = QString("%1")
                     .arg( (txFreq.frequencyAssigned + txFreq.frequencyUpperBand) / 2);
        } else {
          _freq = QString("%1").arg(txFreq.frequencyAssigned);
        }
        row << _freq; // Center Frequency (MHz)
      }
      
      {
        if (txEmFound) {
          row << makeNumber(bwMhz); // Bandiwdth (MHz)
        } else {
          row << "";
        }
        row << makeNumber(lowFreq); // Lower Band (MHz)
        row << makeNumber(highFreq); // Upper Band (MHz)
      }

      row << makeNumber(txFreq.tolerance);               // Tolerance (%)
      row << makeNumber(txFreq.EIRP);                    // Tx EIRP (dBm)
      row << charString(txFreq.transmitterPowerControl); // Auto Tx Pwr Control
      if (txEmFound) {
        row << QString(e.desig);      // Emissions Designator
        row << makeNumber(e.modRate); // Digital Mod Rate
        row << QString(e.modCode);    // Digital Mod Type
      } else {
        row << ""
            << ""
            << "";
      }

      row << txFreq.transmitterMake;  // Tx Manufacturer
      row << txFreq.transmitterModel; // Tx Model
      row << txLoc.locationName;      // Tx Location Name
      row << makeNumber(txLoc.latitude);
      // QString("%1-%2-%3
      // %4").arg(txLoc.latitudeDeg).arg(txLoc.latitudeMinutes).arg(txLoc.latitudeSeconds).arg(txLoc.latitudeDirection);
      // // Tx Lat Coords
      row << makeNumber(txLoc.longitude);
      // QString("%1-%2-%3
      // %4").arg(txLoc.longitudeDeg).arg(txLoc.longitudeMinutes).arg(txLoc.longitudeSeconds).arg(txLoc.longitudeDirection);
      // // Tx Lon Coords
      row << makeNumber(txLoc.groundElevation); // Tx Ground Elevation (m)
      row << txAnt.polarizationCode;            // Tx Polarization
      row << makeNumber(txAnt.azimuth);         // Tx Azimuth Angle (deg)
      row << makeNumber(txAnt.tilt);            // Tx Elevation Angle (deg)
      row << txAnt.antennaMake;                 // Tx Ant Manufacturer
      row << txAnt.antennaModel;                // Tx Ant Model
      row << txAntennaModelName.c_str();            // Tx Matched antenna model (blank if unmatched)
      row << AntennaModelClass::categoryStr(txAntennaCategory).c_str(); // Tx Antenna category
      row << makeNumber(txAntennaDiameter);     // Tx Ant Diameter (m)
      row << makeNumber(txAntennaMidbandGain);  // Tx Ant Midband Gain (dB)
      row << makeNumber(txAnt.heightToCenterRAAT); // Tx Height to Center RAAT (m)
      row << makeNumber(txAnt.beamwidth); // Tx Beamwidth
      row << makeNumber(txAnt.gain);      // Tx Gain (dBi)
      row << rxLoc.locationName;          // Rx Location Name
      row << makeNumber(rxLoc.latitude);
      // QString("%1-%2-%3
      // %4").arg(rxLoc.latitudeDeg).arg(rxLoc.latitudeMinutes).arg(rxLoc.latitudeSeconds).arg(rxLoc.latitudeDirection);
      // // Rx Lat Coords
      row << makeNumber(rxLoc.longitude);
      // QString("%1-%2-%3
      // %4").arg(rxLoc.longitudeDeg).arg(rxLoc.longitudeMinutes).arg(rxLoc.longitudeSeconds).arg(rxLoc.longitudeDirection);
      // // Rx Lon Coords
      row << makeNumber(rxLoc.groundElevation); // Rx Ground Elevation (m)
      row << "";                                // Rx Manufacturer
      row << "";                                // Rx Model
      row << rxAnt.antennaMake;                 // Rx Ant Manufacturer
      row << rxAnt.antennaModel;                // Rx Ant Model
      row << rxAntennaModelName.c_str();            // Rx Matched antenna model (blank if unmatched)
      row << AntennaModelClass::categoryStr(rxAntennaCategory).c_str(); // Rx Antenna category
      row << makeNumber(rxAntennaDiameter);     // Rx Ant Diameter (m)
      row << makeNumber(rxAntennaMidbandGain);  // Rx Ant Midband Gain (dB)
      row << makeNumber(rxAnt.lineLoss);        // Rx Line Loss (dB)
      row << makeNumber(rxAnt.heightToCenterRAAT);  // Rx Height to Center RAAT (m)
      row << makeNumber(rxAnt.gain);            // Rx Gain (dBi)
      row << makeNumber(rxAnt.diversityHeight); // Rx Diveristy Height (m)
      row << makeNumber(rxAnt.diversityGain);   // Rx Diversity Gain (dBi)

      row << QString::number(prLocList.size());
      for(prIdx=1; prIdx<=maxNumPassiveRepeater; ++prIdx) {

          if (    (prIdx <= prLocList.size())
               && (prIdx <= prAntList.size()) ) {
              UlsLocation &prLoc = prLocList[prIdx-1];
              UlsAntenna &prAnt = prAntList[prIdx-1];
              UlsSegment &segment = segList[prIdx];

			  AntennaModelClass *prAntModel = antennaModelMap.find(std::string(prAnt.antennaModel));

              AntennaModelClass::TypeEnum prAntennaType;
              AntennaModelClass::CategoryEnum prAntennaCategory;
              double prAntennaDiameter;
              double prAntennaMidbandGain;
              double prAntennaReflectorWidth;
              double prAntennaReflectorHeight;
              std::string prAntennaModelName;

              if (prAntModel) {
                  numAntMatch++;
                  prAntennaModelName = prAntModel->name;
                  prAntennaType = prAntModel->type;
                  prAntennaCategory = prAntModel->category;
                  prAntennaDiameter = prAntModel->diameterM;
                  prAntennaMidbandGain = prAntModel->midbandGain;
                  prAntennaReflectorWidth = prAntModel->reflectorWidthM;
                  prAntennaReflectorHeight = prAntModel->reflectorHeightM;
              } else {
                  numAntUnmatch++;
                  prAntennaModelName = "";
                  prAntennaType = AntennaModelClass::UnknownType;
                  prAntennaCategory = AntennaModelClass::UnknownCategory;
                  prAntennaDiameter = -1.0;
                  prAntennaMidbandGain = std::numeric_limits<double>::quiet_NaN();
                  prAntennaReflectorWidth = -1.0;
                  prAntennaReflectorHeight = -1.0;
                  fixedReason.append("PR Antenna Model Unmatched");
              }

              row << prLoc.locationName;          // Passive Repeater Location Name
              row << makeNumber(prLoc.latitude);  // Passive Repeater Lat Coords
              row << makeNumber(prLoc.longitude); // Passive Repeater Lon Coords
              row << makeNumber(prLoc.groundElevation);       // Passive Repeater Ground Elevation
              row << prAnt.polarizationCode;    // Passive Repeater Polarization
              row << makeNumber(prAnt.azimuth); // Passive Repeater Azimuth Angle
              row << makeNumber(prAnt.tilt);    // Passive Repeater Elevation Angle
              row << prAnt.antennaMake;         // Passive Repeater Ant Make
              row << prAnt.antennaModel;        // Passive Repeater Ant Model
              row << prAntennaModelName.c_str();        // Passive Repeater antenna model (blank if unmatched)
              row << AntennaModelClass::typeStr(prAntennaType).c_str(); // Passive Repeater Ant Type
              row << AntennaModelClass::categoryStr(prAntennaCategory).c_str(); // Passive Repeater Ant Category
              row << makeNumber(prAnt.backtobackTxGain); // Passive Repeater Back-To-Back Tx Gain
              row << makeNumber(prAnt.backtobackRxGain); // Passive Repeater Back-To-Back Rx Gain
              row << makeNumber(prAnt.reflectorHeight); // Passive Repeater ULS Reflector Height
              row << makeNumber(prAnt.reflectorWidth);  // Passive Repeater ULS Reflector Width
              row << makeNumber(prAntennaDiameter);     // Passive Repeater Ant Model Diameter (m)
              row << makeNumber(prAntennaMidbandGain);  // Passive Repeater Ant Model Midband Gain (dB)
              row << makeNumber(prAntennaReflectorHeight); // Passive Repeater Ant Model Reflector Height
              row << makeNumber(prAntennaReflectorWidth);  // Passive Repeater Ant Model Reflector Width
              row << makeNumber(prAnt.lineLoss);        // Passive Repeater Line Loss
              row << makeNumber(prAnt.heightToCenterRAAT); // Passive Repeater Height to Center RAAT
              row << makeNumber(prAnt.beamwidth); // Passive Repeater Beamwidth
              row << makeNumber(segment.segmentLength); // Segment Length (km)
          } else {
              row << "";
              row << "";
              row << "";
              row << "";
              row << "";
              row << "";
              row << "";
              row << "";
              row << "";
              row << "";
              row << "";
              row << "";
              row << "";
              row << "";
              row << "";
              row << "";
              row << "";
              row << "";
              row << "";
              row << "";
              row << "";
              row << "";
              row << "";
              row << "";
          }
      }
      if(anomalousReason.length() > 0) {
        row << "0" << anomalousReason; 
        anomalous.writeRow(row);
        anomalousReason = "";
      } else {
        wt.writeRow(row);
        if (fixedReason.length() > 0) {
            row << "1" << fixedReason; 
            anomalous.writeRow(row);
        }
        numRecs++;
      }
      fixedReason = "";
      
    }
    }
  }

    if (fwarn) {
        fclose(fwarn);
    }

    std::cout << "Num Antenna Matched: " << numAntMatch << std::endl;
    std::cout << "Num Antenna Not Matched: " << numAntUnmatch << std::endl;
    std::cout << "NUM Missing Rx Antenna Height: " << numMissingRxAntHeight << std::endl;
    std::cout << "NUM Missing Tx Antenna Height: " << numMissingTxAntHeight << std::endl;

    std::cout<<"Processed " << r.frequencies().count()
             << " frequency records and output to file; a total of " << numRecs
             << " output"<<'\n';

    time_t t2 = time(NULL);
    tstr = strdup(ctime(&t2));
    strtok(tstr, "\n");
    std::cout << tstr << " : Completed processing." << std::endl;
    free(tstr);

    int elapsedTime = (int) (t2-t1);

    int et = elapsedTime;
    int elapsedTimeSec = et % 60;
    et = et / 60;
    int elapsedTimeMin = et % 60;
    et = et / 60;
    int elapsedTimeHour = et % 24;
    et = et / 24;
    int elapsedTimeDay = et;

    std::cout << "Elapsed time = " << (t2-t1) << " sec = ";
    if (elapsedTimeDay) {
        std::cout << elapsedTimeDay  << " days ";
    }
    if (elapsedTimeDay || elapsedTimeHour) {
        std::cout << elapsedTimeHour << " hours ";
    }
    std::cout << elapsedTimeMin  << " min ";
    std::cout << elapsedTimeSec  << " sec";
    std::cout << std::endl;
}
/******************************************************************************************/

/******************************************************************************************/
/**** testAntennaModelMap                                                              ****/
/******************************************************************************************/
void testAntennaModelMap(AntennaModelMapClass &antennaModelMap, std::string inputFile, std::string outputFile)
{
    char *chptr;
    std::ostringstream errStr;
    FILE *fin, *fout;

    if ( !(fin = fopen(inputFile.c_str(), "rb")) ) {
        errStr << std::string("ERROR: Unable to open inputFile: \"") << inputFile << "\"" << std::endl;
        throw std::runtime_error(errStr.str());
    }

    if ( !(fout = fopen(outputFile.c_str(), "wb")) ) {
        errStr << std::string("ERROR: Unable to open outputFile: \"") << outputFile << "\"" << std::endl;
        throw std::runtime_error(errStr.str());
    }

    int linenum, fIdx;
    std::string line, strval;

    int antennaModelFieldIdx = -1;

    std::vector<int *> fieldIdxList;                       std::vector<std::string> fieldLabelList;
    fieldIdxList.push_back(&antennaModelFieldIdx);         fieldLabelList.push_back("antennaModel");

    int fieldIdx;

    enum LineTypeEnum {
         labelLineType,
          dataLineType,
        ignoreLineType,
       unknownLineType
    };

    LineTypeEnum lineType;

    linenum = 0;
    bool foundLabelLine = false;
    while (fgetline(fin, line, false)) {
        linenum++;
        std::vector<std::string> fieldList = splitCSV(line);
        std::string fixedStr = "";

        lineType = unknownLineType;
        /**************************************************************************/
        /**** Determine line type                                              ****/
        /**************************************************************************/
        if (fieldList.size() == 0) {
            lineType = ignoreLineType;
        } else {
            fIdx = fieldList[0].find_first_not_of(' ');
            if (fIdx == (int) std::string::npos) {
                if (fieldList.size() == 1) {
                    lineType = ignoreLineType;
                }
            } else {
                if (fieldList[0].at(fIdx) == '#') {
                    lineType = ignoreLineType;
                }
            }
        }

        if ((lineType == unknownLineType)&&(!foundLabelLine)) {
            lineType = labelLineType;
            foundLabelLine = 1;
        }
        if ((lineType == unknownLineType)&&(foundLabelLine)) {
            lineType = dataLineType;
        }
        /**************************************************************************/

        /**************************************************************************/
        /**** Process Line                                                     ****/
        /**************************************************************************/
        bool found;
        std::string field;
        int xIdx, yIdx;
        switch(lineType) {
            case   labelLineType:
                for(fieldIdx=0; fieldIdx<(int) fieldList.size(); fieldIdx++) {
                    field = fieldList.at(fieldIdx);

                    // std::cout << "FIELD: \"" << field << "\"" << std::endl;

                    found = false;
                    for(fIdx=0; (fIdx < (int) fieldLabelList.size())&&(!found); fIdx++) {
                        if (field == fieldLabelList.at(fIdx)) {
                            *fieldIdxList.at(fIdx) = fieldIdx;
                            found = true;
                        }
                    }
                }

                for(fIdx=0; fIdx < (int) fieldIdxList.size(); fIdx++) {
                    if (*fieldIdxList.at(fIdx) == -1) {
                        errStr << "ERROR: Invalid input file \"" << inputFile << "\" label line missing \"" << fieldLabelList.at(fIdx) << "\"" << std::endl;
                        throw std::runtime_error(errStr.str());
                    }
                }

                fprintf(fout, "%s,matchedAntennaModel\n", line.c_str());

                break;
            case    dataLineType:
                {
                    strval = fieldList.at(antennaModelFieldIdx);

                    AntennaModelClass *antModel = antennaModelMap.find(strval);

                    std::string matchedModelName;
                    if (antModel) {
                        matchedModelName = antModel->name;
                    } else {
                        matchedModelName = "";
                    }

                    fprintf(fout, "%s,%s\n", line.c_str(), matchedModelName.c_str());

                }
                break;
            case  ignoreLineType:
            case unknownLineType:
                // do nothing
                break;
            default:
                CORE_DUMP;
                break;
        }
    }

    if (fin) { fclose(fin); }
    if (fout) { fclose(fout); }
}
/******************************************************************************************/

