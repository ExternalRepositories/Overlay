#include "Merger.h"
// #include "Exceptions.h"
#include "marlin/Processor.h"

// #include "lcio.h"
#include "IMPL/LCCollectionVec.h"
#include "IMPL/SimCalorimeterHitImpl.h"
#include "IMPL/CalorimeterHitImpl.h"
#include "IMPL/MCParticleImpl.h"


#include "UTIL/LCTOOLS.h"
#include "UTIL/Operators.h"
#include "UTIL/BitSet32.h"

// #include "IMPL/TrackerHitImpl.h" 

// #include "IMPL/SimTrackerHitImpl.h" 
#include "FPCCDData.h"

// using namespace EVENT ;
// using namespace IMPL ;
// using namespace marlin ;

// #include <iostream>

 using namespace std ;
#include <algorithm>

namespace overlay{
  
  inline long long cellID2long(int id0, int id1) { return ((long long) id0 << 32) | id1; }
    

  void Merger::mergeMC(LCEvent* srcEvent, LCEvent* destEvent, string mcDestString) {
    int numMCcollections = 0;
    string mcSrcString;
    
    const vector<string>* srcColNames = srcEvent->getCollectionNames();
    vector<string>::const_iterator srcEndIt = srcColNames->end();
    for (vector<string>::const_iterator it = srcColNames->begin() ; it < srcEndIt ; it++ ) {
      if( (srcEvent->getCollection(*it))->getTypeName() == LCIO::MCPARTICLE ){
        numMCcollections++;
        mcSrcString = *it;
      }
    }
    if (numMCcollections == 1) {
      Merger::mergeMC(srcEvent, mcSrcString, destEvent, mcDestString);
    } else { 
      streamlog_out( WARNING ) << "Source collection number of MC Collections != 1" << endl;
    }
    return;
  }
  
  
  void Merger::mergeMC(LCEvent* srcEvent, string mcSrcString, LCEvent* destEvent, string mcDestString) {
    
    try {
      destEvent->getCollection(mcDestString);
    }
    catch (DataNotAvailableException& e) {   // if (does_not_exist(mcDestCollection)) {
      LCCollectionVec* mcVec = new LCCollectionVec( LCIO::MCPARTICLE )  ;
      destEvent->addCollection( mcVec , mcDestString ) ;
    }
     
    try {
      Merger::merge(srcEvent->getCollection(mcSrcString), destEvent->getCollection(mcDestString));
    }
    catch (exception& e) {
      streamlog_out( WARNING ) << "Failed to merge montecarlo collections" << endl;
      return;
    }
    srcEvent->removeCollection(mcSrcString);
    Merger::merge(srcEvent, destEvent);
    return;
  }
  
  
  void Merger::merge(LCEvent* srcEvent, LCEvent* destEvent) {
    const vector<string>* srcColNames = srcEvent->getCollectionNames();
    const vector<string>* destColNames = destEvent->getCollectionNames();
    
    vector<string>::const_iterator destBeginIt = destColNames->begin();
    vector<string>::const_iterator destEndIt   = destColNames->end();
    vector<string>::const_iterator srcEndIt    = srcColNames->end();
    for (vector<string>::const_iterator it = srcColNames->begin() ; it < srcEndIt ; it++ ) {
      if (destEndIt != find(destBeginIt, destEndIt, *it)) {

        Merger::merge(srcEvent->getCollection(*it), destEvent->getCollection(*it));
        
      }
    }
    return;
  }
  
  
  void Merger::merge(LCEvent* srcEvent, LCEvent* destEvent, map<string, string> *mergeMap) {
    
    map<string, string>::iterator it;
    map<string, string>::iterator endIt = mergeMap->end();
    for ( it=mergeMap->begin() ; it != endIt ; it++ ) {
      LCCollection *srcCol, *destCol;
      
      try {
        srcCol = srcEvent->getCollection((*it).first);
      } catch (DataNotAvailableException& e) {


	// streamlog_out( DEBUG ) << "The source collection " << (*it).first 
	//                        << " does not exist." << endl;

        continue;
      }
      
      try {

        destCol = destEvent->getCollection((*it).second);

      } catch (DataNotAvailableException& e) {

        streamlog_out( DEBUG ) << "destination collection " << (*it).second  << " was created." << endl;
	
        destCol = new LCCollectionVec( srcCol->getTypeName() ) ;

	// fg: we need to copy all collection parameters from the source collection  
	
	//fg: does not work :	destCol->parameters() = srcCol->getParameters() ;
	// -> do it 'manually':

	StringVec stringKeys ;
	srcCol->getParameters().getStringKeys( stringKeys ) ;
	for(unsigned i=0; i< stringKeys.size() ; i++ ){
	  StringVec vals ;
	  srcCol->getParameters().getStringVals(  stringKeys[i] , vals ) ;
	  destCol->parameters().setValues(  stringKeys[i] , vals ) ;   
	}
	StringVec intKeys ;
	srcCol->getParameters().getIntKeys( intKeys ) ;
	for(unsigned i=0; i< intKeys.size() ; i++ ){
	  IntVec vals ;
	  srcCol->getParameters().getIntVals(  intKeys[i] , vals ) ;
	  destCol->parameters().setValues(  intKeys[i] , vals ) ;   
	}
	StringVec floatKeys ;
	srcCol->getParameters().getFloatKeys( floatKeys ) ;
	for(unsigned i=0; i< floatKeys.size() ; i++ ){
	  FloatVec vals ;
	  srcCol->getParameters().getFloatVals(  floatKeys[i] , vals ) ;
	  destCol->parameters().setValues(  floatKeys[i] , vals ) ;   
	}
	
	streamlog_out( DEBUG ) <<  " copied collection parameters ... " << std::endl ;
	streamlog_message( DEBUG , 
			   LCTOOLS::printParameters( srcCol->getParameters() ) ;
			   LCTOOLS::printParameters( destCol->getParameters() ) ;
			   ,"\n" ;  ) ;
	  
	destEvent->addCollection( destCol , (*it).second ) ;
      }
      
      Merger::merge(srcCol, destCol);
      
    }
    return;
  }

  
  void Merger::merge(LCEvent* srcEvent, string srcString, LCEvent* destEvent, string destString) {
    try {
      Merger::merge(srcEvent->getCollection(srcString), destEvent->getCollection(destString));
    }
    catch (exception& e) {
      streamlog_out( WARNING ) << e.what() << endl;
    }
    return;
  }
  
  
  void Merger::merge(LCCollection* src, LCCollection* dest) {
    int nElementsSrc, nElementsDest;
    const string destType = dest->getTypeName();
    
    // check if collections have the same type
    if (destType != src->getTypeName()) {
      streamlog_out( WARNING ) << "merge not possible, collections of different type" << endl;
      return;
    }
    
    streamlog_out( DEBUG4 ) << "merging collection of type: " << destType << " --- \n";
    
    // ** "TRACKERHITS" **
    if ((destType == LCIO::SIMTRACKERHIT) || (destType == LCIO::TRACKERHIT))  {

      // running trough all the elements in the collection.
      nElementsSrc = src->getNumberOfElements();

      streamlog_out( DEBUG4 ) << "merging ...  nElements = " << nElementsSrc << endl;
      
      for (int i=nElementsSrc-1; i>=0; i--) {

        dest->addElement( src->getElementAt(i) );
	src->removeElementAt(i);

      }
      return;
    }
    // **MCPARTICLE  **
    if ( destType == LCIO::MCPARTICLE  )  {

      // running trough all the elements in the collection.
      nElementsSrc = src->getNumberOfElements();

      streamlog_out( DEBUG2 ) << "merging ... nMCParticles = " << nElementsSrc << endl;
      
      for (int i=nElementsSrc-1; i>=0; i--) {
	
	MCParticleImpl* p =  dynamic_cast<MCParticleImpl*>( src->getElementAt(i) ) ;
	
	//	p->setSimulatorStatus( set_bit(  p->getSimulatorStatus() ,  BITOverlay  )  ) ;
	p->setOverlay( true ) ;

	streamlog_out( DEBUG2 ) << " --- " <<  lcshort( (MCParticle*) p , src )   ; // <<  BitSet32(  p->getSimulatorStatus() )  ;
	
        dest->addElement( p );
	src->removeElementAt(i);

      }
      return;
    }
    
    // ** LCGENERICOBJECT-VTXPixelHits **
    if( destType == LCIO::LCGENERICOBJECT){
      streamlog_out( DEBUG4 ) << "merging" << endl;
      int nLayer = 6;
      int maxLadder = 17;
      FPCCDData srcData( nLayer, maxLadder);
      FPCCDData destData( nLayer, maxLadder);
      
      int nSrcHits = srcData.unpackPixelHits( *src );
      int nDestHits = destData.unpackPixelHits( *dest );

      streamlog_out( DEBUG ) << "number of pixel hits : src-" << nSrcHits << ", dest-" << nDestHits << endl;
      
      destData.Add(srcData);
      srcData.clear();
      int nElementsDest = dest->getNumberOfElements();
      for(int i=nElementsDest-1 ; i>=0 ; i--){
	dest->removeElementAt(i);
      }
      int nElementsSrc = src->getNumberOfElements();
      for(int i=nElementsSrc-1 ; i>=0 ; i--){
	src->removeElementAt(i);
      }
      destData.packPixelHits( *dest );
      return;
    }
    
    // ** SIMCALORIMETERHIT **
    if (destType == LCIO::SIMCALORIMETERHIT ) {
      SimCalorimeterHitImpl *srcHit, *destHit;
      
      streamlog_out( DEBUG ) << "merging" << endl;
      nElementsSrc = src->getNumberOfElements();
      nElementsDest = dest->getNumberOfElements();
      
      // create a map of dest Collection
      map<long long, SimCalorimeterHitImpl*> destMap;
      map<long long, SimCalorimeterHitImpl*>::iterator destMapIt;
      pair<map<long long, SimCalorimeterHitImpl*>::iterator,bool> res;
      for (int i=0; i<nElementsDest; i++) {
        destHit = dynamic_cast<SimCalorimeterHitImpl*> ( dest->getElementAt(i) );
        res = destMap.insert( pair<long long, SimCalorimeterHitImpl*>(cellID2long(destHit->getCellID0(), destHit->getCellID1()), destHit) );
      }

      // process the src collection and merge with dest
      for (int i=nElementsSrc-1; i>=0 ; i--) {
        srcHit = dynamic_cast<SimCalorimeterHitImpl*> ( src->getElementAt(i) );
        if ((destMapIt = destMap.find(cellID2long(srcHit->getCellID0(), srcHit->getCellID1()))) == destMap.end()) {
          dest->addElement( srcHit );
        } else {
          int numMC = srcHit->getNMCContributions();
          
          for( int j=0 ; j<numMC ; j++){
            destMapIt->second->addMCParticleContribution( srcHit->getParticleCont(j), srcHit->getEnergyCont(j), srcHit->getTimeCont(j), srcHit->getPDGCont(j));
          }
          
          delete srcHit;
        }
        src->removeElementAt(i);
      }
      return;
    }
    
    // ** CALORIMETERHIT **
    if (destType == LCIO::CALORIMETERHIT ) {
      CalorimeterHitImpl *srcHit, *destHit;
    
      streamlog_out( DEBUG ) << "merging" << endl;
      nElementsSrc = src->getNumberOfElements();
      nElementsDest = dest->getNumberOfElements();
      
      // create a map of dest Collection
      map<long long, CalorimeterHitImpl *> destMap;
      map<long long, CalorimeterHitImpl *>::iterator destMapIt;
      pair<map<long long, CalorimeterHitImpl *>::iterator,bool> res;
      for (int i=0; i<nElementsDest; i++) {
        destHit = dynamic_cast<CalorimeterHitImpl *> ( dest->getElementAt(i) );
        res = destMap.insert( pair<long long, CalorimeterHitImpl *>(cellID2long(destHit->getCellID0(), destHit->getCellID1()), destHit) );
      }

      // process the src collection and merge with dest
      for (int i=nElementsSrc-1; i>=0 ; i--) {
        srcHit = dynamic_cast<CalorimeterHitImpl *> ( src->getElementAt(i) );
        if ((destMapIt = destMap.find(cellID2long(srcHit->getCellID0(), srcHit->getCellID1()))) == destMap.end()) {
          dest->addElement( srcHit );
        } else {
          destMapIt->second->setEnergy( destMapIt->second->getEnergy() + srcHit->getEnergy() );
        }
        src->removeElementAt(i);
      }
      return;
    }
    
    // ** DEFAULT **
    streamlog_out( DEBUG ) << "merge not possible for this type" << endl;
    return;
  }

}
