// Written by Oliver Serang 2009
// see license for more information

#include "ProteinIdentifier.h"

ProteinIdentifier::ProteinIdentifier()
{
  
  //TODO these guys should be parametizable
  ProteinThreshold = 1e-5;
  //ProteinThreshold = 0.0;
  
  //  PeptideThreshold = 1e-9;
  PeptideThreshold = 9e-3;
  //PeptideThreshold = 0.0;
  
  PsmThreshold = 0.0;
  
  //  PeptidePrior = .5;
  PeptidePrior = .07384;
}

