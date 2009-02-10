/* ****************************************************************** **
**    OpenFRESCO - Open Framework                                     **
**                 for Experimental Setup and Control                 **
**                                                                    **
**                                                                    **
** Copyright (c) 2006, The Regents of the University of California    **
** All Rights Reserved.                                               **
**                                                                    **
** Commercial use of this program without express permission of the   **
** University of California, Berkeley, is strictly prohibited. See    **
** file 'COPYRIGHT_UCB' in main directory for information on usage    **
** and redistribution, and for a DISCLAIMER OF ALL WARRANTIES.        **
**                                                                    **
** Developed by:                                                      **
**   Andreas Schellenberg (andreas.schellenberg@gmx.net)              **
**   Yoshikazu Takahashi (yos@catfish.dpri.kyoto-u.ac.jp)             **
**   Gregory L. Fenves (fenves@berkeley.edu)                          **
**   Stephen A. Mahin (mahin@berkeley.edu)                            **
**                                                                    **
** ****************************************************************** */

// $Revision$
// $Date$
// $URL: $

// Written: Andreas Schellenberg (andreas.schellenberg@gmx.net)
// Created: 09/06
// Revision: A
//
// Description: This file contains the implementation of the EETwoNodeLink class.

#include "EETwoNodeLink.h"

#include <Domain.h>
#include <Node.h>
#include <Channel.h>
#include <FEM_ObjectBroker.h>
#include <Renderer.h>
#include <Information.h>
#include <ElementResponse.h>
#include <TCP_Socket.h>
#include <TCP_SocketSSL.h>

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>


// initialise the class wide variables
Matrix EETwoNodeLink::EETwoNodeLinkM2(2,2);
Matrix EETwoNodeLink::EETwoNodeLinkM4(4,4);
Matrix EETwoNodeLink::EETwoNodeLinkM6(6,6);
Matrix EETwoNodeLink::EETwoNodeLinkM12(12,12);
Vector EETwoNodeLink::EETwoNodeLinkV2(2);
Vector EETwoNodeLink::EETwoNodeLinkV4(4);
Vector EETwoNodeLink::EETwoNodeLinkV6(6);
Vector EETwoNodeLink::EETwoNodeLinkV12(12);


// responsible for allocating the necessary space needed
// by each object and storing the tags of the end nodes.
EETwoNodeLink::EETwoNodeLink(int tag, int dim, int Nd1, int Nd2, 
    const ID &direction, const Vector _y, const Vector _x,
    ExperimentalSite *site, Vector Mr, bool iM, double m)
    : ExperimentalElement(tag, ELE_TAG_EETwoNodeLink, site),     
    dimension(dim), numDOF(0), connectedExternalNodes(2),
    numDir(direction.Size()), dir(0), trans(3,3),
    x(_x), y(_y), Mratio(Mr), iMod(iM), mass(m), L(0.0),
    db(0), vb(0), ab(0), t(0),
    dbMeas(0), vbMeas(0), abMeas(0), qMeas(0), tMeas(0),
    dbTarg(direction.Size()), vbTarg(direction.Size()),
    abTarg(direction.Size()), dl(0), Tgl(0,0), Tlb(0,0),
    dbPast(direction.Size()), tPast(0.0),
    kbInit(direction.Size(), direction.Size()),
    theMatrix(0), theVector(0), theLoad(0),
    firstWarning(true)
{
    // ensure the connectedExternalNode ID is of correct size & set values
    if (connectedExternalNodes.Size() != 2)  {
        opserr << "EETwoNodeLink::EETwoNodeLink() - element: "
            << this->getTag() << " failed to create an ID of size 2\n";
        exit(-1);
    }
    
    connectedExternalNodes(0) = Nd1;
    connectedExternalNodes(1) = Nd2;
    
    // set node pointers to NULL
    for (int i=0; i<2; i++)
        theNodes[i] = 0;
    
    // check the number of directions
    if (numDir < 1 || numDir > 12)  {
        opserr << "EETwoNodeLink::EETwoNodeLink() - element: "
            << this->getTag() << " wrong number of directions\n";
        exit(-1);
    }
    
    // allocate memory for direction array
    dir = new ID(numDir);
    if (dir == 0)  {
        opserr << "EETwoNodeLink::EETwoNodeLink() - "
            << "failed to creat direction array\n";
        exit(-1);
    }
    
    // initialize directions and check for valid values
    (*dir) = direction;
    for (int i=0; i<numDir; i++)  {
        if ((*dir)(i) < 0 || (*dir)(i) > 5)  {
            opserr << "EETwoNodeLink::EETwoNodeLink() - "
                << "incorrect direction " << (*dir)(i)
                << " is set to 0\n";
            (*dir)(i) = 0;
        }
    }
    
    // check p-delta moment distribution ratios
    if (Mratio.Size() == 4)  {
        if (Mratio(0)+Mratio(1) > 1.0)  {
            opserr << "EETwoNodeLink::EETwoNodeLink() - "
                << "incorrect p-delta moment ratios:\nrMy1 + rMy2 = "
                << Mratio(0)+Mratio(1) << " > 1.0\n";
            exit(-1);
        }
        if (Mratio(2)+Mratio(3) > 1.0)  {
            opserr << "EETwoNodeLink::EETwoNodeLink() - "
                << "incorrect p-delta moment ratios:\nrMz1 + rMz2 = "
                << Mratio(2)+Mratio(3) << " > 1.0\n";
            exit(-1);
        }
    }
    
    // set the data size for the experimental site
    sizeCtrl = new ID(OF_Resp_All);
    sizeDaq = new ID(OF_Resp_All);
    
    (*sizeCtrl)[OF_Resp_Disp]  = numDir;
    (*sizeCtrl)[OF_Resp_Vel]   = numDir;
    (*sizeCtrl)[OF_Resp_Accel] = numDir;
    (*sizeCtrl)[OF_Resp_Time]  = 1;
    
    (*sizeDaq)[OF_Resp_Disp]   = numDir;
    (*sizeDaq)[OF_Resp_Vel]    = numDir;
    (*sizeDaq)[OF_Resp_Accel]  = numDir;
    (*sizeDaq)[OF_Resp_Force]  = numDir;
    (*sizeDaq)[OF_Resp_Time]   = 1;
    
    theSite->setSize(*sizeCtrl, *sizeDaq);
    
    // allocate memory for trial response vectors
    db = new Vector(numDir);
    vb = new Vector(numDir);
    ab = new Vector(numDir);
    t  = new Vector(1);

    // allocate memory for measured response vectors
    dbMeas = new Vector(numDir);
    vbMeas = new Vector(numDir);
    abMeas = new Vector(numDir);
    qMeas  = new Vector(numDir);
    tMeas  = new Vector(1);

    // initialize additional vectors
    dbTarg.Zero();
    vbTarg.Zero();
    abTarg.Zero();
    dbPast.Zero();
}


// responsible for allocating the necessary space needed
// by each object and storing the tags of the end nodes.
EETwoNodeLink::EETwoNodeLink(int tag, int dim, int Nd1, int Nd2, 
    const ID &direction, const Vector _y, const Vector _x,
    int port, char *machineInetAddr, int ssl, int dataSize,
    Vector Mr, bool iM, double m)
    : ExperimentalElement(tag, ELE_TAG_EETwoNodeLink),     
    dimension(dim), numDOF(0), connectedExternalNodes(2),
    numDir(direction.Size()), dir(0), trans(3,3),
    x(_x), y(_y), Mratio(Mr), iMod(iM), mass(m), L(0.0),
    theChannel(0), sData(0), sendData(0), rData(0), recvData(0),
    db(0), vb(0), ab(0), t(0),
    dbMeas(0), vbMeas(0), abMeas(0), qMeas(0), tMeas(0),
    dbTarg(direction.Size()), vbTarg(direction.Size()),
    abTarg(direction.Size()), dl(0), Tgl(0,0), Tlb(0,0),
    dbPast(direction.Size()), tPast(0.0),
    kbInit(direction.Size(), direction.Size()),
    theMatrix(0), theVector(0), theLoad(0),
    firstWarning(true)
{
    // ensure the connectedExternalNode ID is of correct size & set values
    if (connectedExternalNodes.Size() != 2)  {
        opserr << "EETwoNodeLink::EETwoNodeLink() - element: "
            << this->getTag() << " failed to create an ID of size 2\n";
        exit(-1);
    }
    
    connectedExternalNodes(0) = Nd1;
    connectedExternalNodes(1) = Nd2;
    
    // set node pointers to NULL
    for (int i=0; i<2; i++)
        theNodes[i] = 0;
    
    // check the number of directions
    if (numDir < 1 || numDir > 12)  {
        opserr << "EETwoNodeLink::EETwoNodeLink() - element: "
            << this->getTag() << " wrong number of directions\n";
        exit(-1);
    }
    
    // allocate memory for direction array
    dir = new ID(numDir);
    if (dir == 0)  {
        opserr << "EETwoNodeLink::EETwoNodeLink() - "
            << "failed to creat direction array\n";
        exit(-1);
    }
    
    // initialize directions and check for valid values
    (*dir) = direction;
    for (int i=0; i<numDir; i++)  {
        if ((*dir)(i) < 0 || (*dir)(i) > 5)  {
            opserr << "EETwoNodeLink::EETwoNodeLink() - "
                << "incorrect direction " << (*dir)(i)
                << " is set to 0\n";
            (*dir)(i) = 0;
        }
    }
    
    // check p-delta moment distribution ratios
    if (Mratio.Size() == 4)  {
        if (Mratio(0)+Mratio(1) > 1.0)  {
            opserr << "EETwoNodeLink::EETwoNodeLink() - "
                << "incorrect p-delta moment ratios:\nrMy1 + rMy2 = "
                << Mratio(0)+Mratio(1) << " > 1.0\n";
            exit(-1);
        }
        if (Mratio(2)+Mratio(3) > 1.0)  {
            opserr << "EETwoNodeLink::EETwoNodeLink() - "
                << "incorrect p-delta moment ratios:\nrMz1 + rMz2 = "
                << Mratio(2)+Mratio(3) << " > 1.0\n";
            exit(-1);
        }
    }
    
    // setup the connection
    if (!ssl)  {
        if (machineInetAddr == 0)
            theChannel = new TCP_Socket(port, "127.0.0.1");
        else
            theChannel = new TCP_Socket(port, machineInetAddr);
    }
    else  {
        if (machineInetAddr == 0)
            theChannel = new TCP_SocketSSL(port, "127.0.0.1");
        else
            theChannel = new TCP_SocketSSL(port, machineInetAddr);
    }
    if (!theChannel)  {
        opserr << "EETwoNodeLink::EETwoNodeLink() "
            << "- failed to create channel\n";
        exit(-1);
    }
    if (theChannel->setUpConnection() != 0)  {
        opserr << "EETwoNodeLink::EETwoNodeLink() "
            << "- failed to setup connection\n";
        exit(-1);
    }
    delete [] machineInetAddr;
    
    // set the data size for the experimental site
    int intData[2*OF_Resp_All+1];
    ID idData(intData, 2*OF_Resp_All+1);
    sizeCtrl = new ID(intData, OF_Resp_All);
    sizeDaq = new ID(&intData[OF_Resp_All], OF_Resp_All);
    idData.Zero();

    (*sizeCtrl)[OF_Resp_Disp]  = numDir;
    (*sizeCtrl)[OF_Resp_Vel]   = numDir;
    (*sizeCtrl)[OF_Resp_Accel] = numDir;
    (*sizeCtrl)[OF_Resp_Time]  = 1;
    
    (*sizeDaq)[OF_Resp_Disp]   = numDir;
    (*sizeDaq)[OF_Resp_Vel]    = numDir;
    (*sizeDaq)[OF_Resp_Accel]  = numDir;
    (*sizeDaq)[OF_Resp_Force]  = numDir;
    (*sizeDaq)[OF_Resp_Time]   = 1;
    
    if (dataSize < 4*numDir+1) dataSize = 4*numDir+1;
    intData[2*OF_Resp_All] = dataSize;

    theChannel->sendID(0, 0, idData, 0);
    
    // allocate memory for the send vectors
    int id = 1;
    sData = new double [dataSize];
    sendData = new Vector(sData, dataSize);
    db = new Vector(&sData[id], numDir);
    id += numDir;
    vb = new Vector(&sData[id], numDir);
    id += numDir;
    ab = new Vector(&sData[id], numDir);
    id += numDir;
    t = new Vector(&sData[id], 1);
    sendData->Zero();

    // allocate memory for the receive vectors
    id = 0;
    rData = new double [dataSize];
    recvData = new Vector(rData, dataSize);
    dbMeas = new Vector(&rData[id], 1);
    id += numDir;
    vbMeas = new Vector(&rData[id], 1);
    id += numDir;
    abMeas = new Vector(&rData[id], 1);
    id += numDir;
    qMeas = new Vector(&rData[id], 1);
    id += numDir;
    tMeas = new Vector(&rData[id], 1);
    recvData->Zero();

    // initialize additional vectors
    dbTarg.Zero();
    vbTarg.Zero();
    abTarg.Zero();
    dbPast.Zero();
}


// delete must be invoked on any objects created by the object.
EETwoNodeLink::~EETwoNodeLink()
{
    // invoke the destructor on any objects created by the object
    // that the object still holds a pointer to
    if (dir != 0)
        delete dir;
    if (theLoad != 0)
        delete theLoad;

    if (db != 0)
        delete db;
    if (vb != 0)
        delete vb;
    if (ab != 0)
        delete ab;
    if (t != 0)
        delete t;
    
    if (dbMeas != 0)
        delete dbMeas;
    if (vbMeas != 0)
        delete vbMeas;
    if (abMeas != 0)
        delete abMeas;
    if (qMeas != 0)
        delete qMeas;
    if (tMeas != 0)
        delete tMeas;

    if (theSite == 0)  {
        sData[0] = OF_RemoteTest_DIE;
        theChannel->sendVector(0, 0, *sendData, 0);
        
        if (sendData != 0)
            delete sendData;
        if (sData != 0)
            delete [] sData;
        if (recvData != 0)
            delete recvData;
        if (rData != 0)
            delete [] rData;
        if (theChannel != 0)
            delete theChannel;
    }
}


int EETwoNodeLink::getNumExternalNodes() const
{
    return 2;
}


const ID& EETwoNodeLink::getExternalNodes() 
{
    return connectedExternalNodes;
}


Node** EETwoNodeLink::getNodePtrs() 
{
    return theNodes;
}


int EETwoNodeLink::getNumDOF() 
{
    return numDOF;
}


int EETwoNodeLink::getNumBasicDOF() 
{
    return numDir;
}


// to set a link to the enclosing Domain and to set the node pointers.
void EETwoNodeLink::setDomain(Domain *theDomain)
{
    // check Domain is not null - invoked when object removed from a domain
    if (theDomain == 0)  {
        theNodes[0] = 0;
        theNodes[1] = 0;
        
        return;
    }
    
    // set default values for error conditions
    numDOF = 2;
    theMatrix = &EETwoNodeLinkM2;
    theVector = &EETwoNodeLinkV2;
    
    // first set the node pointers
    int Nd1 = connectedExternalNodes(0);
    int Nd2 = connectedExternalNodes(1);
    theNodes[0] = theDomain->getNode(Nd1);
    theNodes[1] = theDomain->getNode(Nd2);
    
    // if can't find both - send a warning message
    if (!theNodes[0] || !theNodes[1])  {
        if (!theNodes[0])  {
            opserr << "EETwoNodeLink::setDomain() - Nd1: "
                << Nd1 << " does not exist in the model for ";
        } else  {
            opserr << "EETwoNodeLink::setDomain() - Nd2: " 
                << Nd2 << " does not exist in the model for ";
        }
        opserr << "EETwoNodeLink ele: " << this->getTag() << endln;
        
        return;
    }
    
    // now determine the number of dof and the dimension
    int dofNd1 = theNodes[0]->getNumberDOF();
    int dofNd2 = theNodes[1]->getNumberDOF();
    
    // if differing dof at the ends - print a warning message
    if (dofNd1 != dofNd2)  {
        opserr << "EETwoNodeLink::setDomain(): nodes " << Nd1 << " and " << Nd2
            << "have differing dof at ends for element: " << this->getTag() << endln;
        return;
    }	
    
    // call the base class method
    this->DomainComponent::setDomain(theDomain);
    
    // now set the number of dof for element and set matrix and vector pointer
    if (dimension == 1 && dofNd1 == 1)  {
        numDOF = 2;
        theMatrix = &EETwoNodeLinkM2;
        theVector = &EETwoNodeLinkV2;
        elemType  = D1N2;
    }
    else if (dimension == 2 && dofNd1 == 2)  {
        numDOF = 4;
        theMatrix = &EETwoNodeLinkM4;
        theVector = &EETwoNodeLinkV4;
        elemType  = D2N4;
    }
    else if (dimension == 2 && dofNd1 == 3)  {
        numDOF = 6;
        theMatrix = &EETwoNodeLinkM6;
        theVector = &EETwoNodeLinkV6;
        elemType  = D2N6;
    }
    else if (dimension == 3 && dofNd1 == 3)  {
        numDOF = 6;
        theMatrix = &EETwoNodeLinkM6;
        theVector = &EETwoNodeLinkV6;
        elemType  = D3N6;
    }
    else if (dimension == 3 && dofNd1 == 6)  {
        numDOF = 12;
        theMatrix = &EETwoNodeLinkM12;
        theVector = &EETwoNodeLinkV12;
        elemType  = D3N12;
    }
    else  {
        opserr << "EETwoNodeLink::setDomain() can not handle "
            << dimension << "dofs at nodes in " << dofNd1 << " d problem\n";
        return;
    }
    
    // set the initial stiffness matrix size
    theInitStiff.resize(numDOF,numDOF);
    theInitStiff.Zero();
    
    // set the local displacement vector size
    dl.resize(numDOF);
    dl.Zero();
    
    // allocate memory for the load vector
    if (theLoad == 0)
        theLoad = new Vector(numDOF);
    else if (theLoad->Size() != numDOF)  {
        delete theLoad;
        theLoad = new Vector(numDOF);
    }
    if (theLoad == 0)  {
        opserr << "EETwoNodeLink::setDomain() - element: " << this->getTag()
            << " out of memory creating vector of size: " << numDOF << endln;
        return;
    }          
    
    // setup the transformation matrix for orientation
    this->setUp();
    
    // set transformation matrix from global to local system
    this->setTranGlobalLocal();

    // set transformation matrix from local to basic system
    this->setTranLocalBasic();
}


int EETwoNodeLink::commitState()
{
    int rValue = 0;
    
    if (theSite != 0)  {
        rValue += theSite->commitState();
    }
    else  {
        sData[0] = OF_RemoteTest_commitState;
        rValue += theChannel->sendVector(0, 0, *sendData, 0);
    }
    
    return rValue;
}


int EETwoNodeLink::update()
{
    int rValue = 0;

    // get current time
    Domain *theDomain = this->getDomain();
    (*t)(0) = theDomain->getCurrentTime();
    
    // get global trial response
    const Vector &dsp1 = theNodes[0]->getTrialDisp();
    const Vector &dsp2 = theNodes[1]->getTrialDisp();
    const Vector &vel1 = theNodes[0]->getTrialVel();
    const Vector &vel2 = theNodes[1]->getTrialVel();
    const Vector &acc1 = theNodes[0]->getTrialAccel();
    const Vector &acc2 = theNodes[1]->getTrialAccel();
    
    int numDOF2 = numDOF/2;
    static Vector dg(numDOF), vg(numDOF), ag(numDOF), vl(numDOF), al(numDOF);
    for (int i=0; i<numDOF2; i++)  {
        dg(i)         = dsp1(i);  vg(i)         = vel1(i);  ag(i)         = acc1(i);
        dg(i+numDOF2) = dsp2(i);  vg(i+numDOF2) = vel2(i);  ag(i+numDOF2) = acc2(i);
    }
    
    // transform response from the global to the local system
    dl = Tgl*dg;
    vl = Tgl*vg;
    al = Tgl*ag;

    // transform response from the local to the basic system
    (*db) = Tlb*dl;
    (*vb) = Tlb*vl;
    (*ab) = Tlb*al;
    
    if ((*db) != dbPast || (*t)(0) != tPast)  {
        // save the displacements and the time
        dbPast = (*db);
        tPast = (*t)(0);
        // set the trial response at the site
        if (theSite != 0)  {
            theSite->setTrialResponse(db, vb, ab, (Vector*)0, t);
        }
        else  {
            sData[0] = OF_RemoteTest_setTrialResponse;
            rValue += theChannel->sendVector(0, 0, *sendData, 0);
        }
    }
    
    return rValue;
}


int EETwoNodeLink::setInitialStiff(const Matrix& kbinit)
{
    if (kbinit.noRows() != numDir || kbinit.noCols() != numDir)  {
        opserr << "EETwoNodeLink::setInitialStiff() - " 
            << "matrix size is incorrect for element: "
            << this->getTag() << endln;
        return -1;
    }
    kbInit = kbinit;
    
    // zero the matrix
    theInitStiff.Zero();
    
    // transform from basic to local system
    static Matrix kl(numDOF,numDOF);
    kl.addMatrixTripleProduct(0.0, Tlb, kbInit, 1.0);
    
    // transform from local to global system
    theInitStiff.addMatrixTripleProduct(0.0, Tgl, kl, 1.0);
    
    return 0;
}


const Matrix& EETwoNodeLink::getTangentStiff()
{
    if (firstWarning == true)  {
        opserr << "\nWARNING EETwoNodeLink::getTangentStiff() - "
            << "Element: " << this->getTag() << endln
            << "TangentStiff cannot be calculated." << endln
            << "Return InitialStiff including GeometricStiff instead." 
            << endln;
        opserr << "Subsequent getTangentStiff warnings will be suppressed." 
            << endln;
        
        firstWarning = false;
    }
    
    // zero the matrix
    theMatrix->Zero();
    
    // get measured resisting forces
    if (theSite != 0)  {
        (*qMeas) = theSite->getForce();
    }
    else  {
        sData[0] = OF_RemoteTest_getForce;
        theChannel->sendVector(0, 0, *sendData, 0);
        theChannel->recvVector(0, 0, *recvData, 0);
    }
    
    // apply optional initial stiffness modification
    if (iMod == true)
        this->applyIMod();
    
    // use elastic force if force from test is zero
    for (int i=0; i<numDir; i++)  {
        if ((*qMeas)(i) == 0.0)
            (*qMeas)(i) = kbInit(i,i) * (*db)(i);
    }
    
    // transform from basic to local system
    static Matrix kl(numDOF,numDOF);
    kl.addMatrixTripleProduct(0.0, Tlb, kbInit, 1.0);
    
    // add geometric stiffness to local stiffness
    if (Mratio.Size() == 4)
        this->addPDeltaStiff(kl);
    
    // transform from local to global system
    theMatrix->addMatrixTripleProduct(0.0, Tgl, kl, 1.0);
    
    return *theMatrix;
}


const Matrix& EETwoNodeLink::getMass()
{
    // zero the matrix
    theMatrix->Zero();
    
    // form mass matrix
    if (mass != 0.0)  {
        double m = 0.5*mass;
        int numDOF2 = numDOF/2;
        for (int i=0; i<dimension; i++)  {
            (*theMatrix)(i,i) = m;
            (*theMatrix)(i+numDOF2,i+numDOF2) = m;
        }
    }
    
    return *theMatrix; 
}


void EETwoNodeLink::zeroLoad()
{
    theLoad->Zero();
}


int EETwoNodeLink::addLoad(ElementalLoad *theLoad, double loadFactor)
{
    opserr <<"EETwoNodeLink::addLoad() - "
        << "load type unknown for element: "
        << this->getTag() << endln;
    
    return -1;
}


int EETwoNodeLink::addInertiaLoadToUnbalance(const Vector &accel)
{
    // check for quick return
    if (mass == 0.0)  {
        return 0;
    }    
    
    // get R * accel from the nodes
    const Vector &Raccel1 = theNodes[0]->getRV(accel);
    const Vector &Raccel2 = theNodes[1]->getRV(accel);
    
    int numDOF2 = numDOF/2;
    if (numDOF2 != Raccel1.Size() || numDOF2 != Raccel2.Size())  {
        opserr << "EETwoNodeLink::addInertiaLoadToUnbalance() - "
            << "matrix and vector sizes are incompatible\n";
        return -1;
    }
    
    // want to add ( - fact * M R * accel ) to unbalance
    // take advantage of lumped mass matrix
    double m = 0.5*mass;
    for (int i=0; i<dimension; i++)  {
        (*theLoad)(i) -= m * Raccel1(i);
        (*theLoad)(i+numDOF2) -= m * Raccel2(i);
    }
    
    return 0;
}


const Vector& EETwoNodeLink::getResistingForce()
{
    // zero the residual
    theVector->Zero();
    
    // get measured resisting forces
    if (theSite != 0)  {
        (*qMeas) = theSite->getForce();
    }
    else  {
        sData[0] = OF_RemoteTest_getForce;
        theChannel->sendVector(0, 0, *sendData, 0);
        theChannel->recvVector(0, 0, *recvData, 0);
    }
    
    // apply optional initial stiffness modification
    if (iMod == true)
        this->applyIMod();
    
    // use elastic force if force from test is zero
    for (int i=0; i<numDir; i++)  {
        if ((*qMeas)(i) == 0.0)
            (*qMeas)(i) = kbInit(i,i) * (*db)(i);
    }
    
    // save corresponding target displacements for recorder
    dbTarg = (*db);
    vbTarg = (*vb);
    abTarg = (*ab);
    
    // determine resisting forces in local system
    static Vector ql(numDOF);
    ql = Tlb^(*qMeas);
    
    // add P-Delta effects to local forces
    if (Mratio.Size() == 4)
        this->addPDeltaForces(ql);
    
    // determine resisting forces in global system
    (*theVector) = Tgl^ql;
    
    // subtract external load
    (*theVector) -= *theLoad;
    
    return *theVector;
}


const Vector& EETwoNodeLink::getResistingForceIncInertia()
{	
    this->getResistingForce();
    
    // add the damping forces if rayleigh damping
    if (alphaM != 0.0 || betaK != 0.0 || betaK0 != 0.0 || betaKc != 0.0)
        (*theVector) += this->getRayleighDampingForces();
    
    // now include the mass portion
    if (mass != 0.0)  {
        const Vector &accel1 = theNodes[0]->getTrialAccel();
        const Vector &accel2 = theNodes[1]->getTrialAccel();    
        
        int numDOF2 = numDOF/2;
        double m = 0.5*mass;
        for (int i=0; i<dimension; i++)  {
            (*theVector)(i) += m * accel1(i);
            (*theVector)(i+numDOF2) += m * accel2(i);
        }
    }
    
    return *theVector;
}


const Vector& EETwoNodeLink::getTime()
{	
    if (theSite != 0)  {
        (*tMeas) = theSite->getTime();
    }
    else  {
        sData[0] = OF_RemoteTest_getTime;
        theChannel->sendVector(0, 0, *sendData, 0);
        theChannel->recvVector(0, 0, *recvData, 0);
    }

    return *tMeas;
}


const Vector& EETwoNodeLink::getBasicDisp()
{	
    if (theSite != 0)  {
        (*dbMeas) = theSite->getDisp();
    }
    else  {
        sData[0] = OF_RemoteTest_getDisp;
        theChannel->sendVector(0, 0, *sendData, 0);
        theChannel->recvVector(0, 0, *recvData, 0);
    }

    return *dbMeas;
}


const Vector& EETwoNodeLink::getBasicVel()
{	
    if (theSite != 0)  {
        (*vbMeas) = theSite->getVel();
    }
    else  {
        sData[0] = OF_RemoteTest_getVel;
        theChannel->sendVector(0, 0, *sendData, 0);
        theChannel->recvVector(0, 0, *recvData, 0);
    }

    return *vbMeas;
}


const Vector& EETwoNodeLink::getBasicAccel()
{	
    if (theSite != 0)  {
        (*abMeas) = theSite->getAccel();
    }
    else  {
        sData[0] = OF_RemoteTest_getAccel;
        theChannel->sendVector(0, 0, *sendData, 0);
        theChannel->recvVector(0, 0, *recvData, 0);
    }

    return *abMeas;
}


int EETwoNodeLink::sendSelf(int commitTag, Channel &theChannel)
{
    // has not been implemented yet.....
    return 0;
}


int EETwoNodeLink::recvSelf(int commitTag, Channel &theChannel,
    FEM_ObjectBroker &theBroker)
{
    // has not been implemented yet.....
    return 0;
}


int EETwoNodeLink::displaySelf(Renderer &theViewer,
    int displayMode, float fact)
{    
    // first determine the end points of the element based on
    // the display factor (a measure of the distorted image)
    const Vector &end1Crd = theNodes[0]->getCrds();
    const Vector &end2Crd = theNodes[1]->getCrds();
    
    const Vector &end1Disp = theNodes[0]->getDisp();
    const Vector &end2Disp = theNodes[1]->getDisp();    
    
    static Vector v1(3);
    static Vector v2(3);
    
    for (int i=0; i<dimension; i++) {
        v1(i) = end1Crd(i) + end1Disp(i)*fact;
        v2(i) = end2Crd(i) + end2Disp(i)*fact;    
    }
    
    return theViewer.drawLine (v1, v2, 1.0, 1.0);
}


void EETwoNodeLink::Print(OPS_Stream &s, int flag)
{
    if (flag == 0)  {
        // print everything
        s << "Element: " << this->getTag() << endln;
        s << "  type: EETwoNodeLink" << endln;
        s << "  iNode: " << connectedExternalNodes(0) 
            << ", jNode: " << connectedExternalNodes(1) << endln;
        if (theSite != 0)
            s << "  ExperimentalSite: " << theSite->getTag() << endln;
        s << "  mass: " << mass << endln;
        // determine resisting forces in global system
        s << "  resisting force: " << this->getResistingForce() << endln;
    } else if (flag == 1)  {
        // does nothing
    }
}


Response* EETwoNodeLink::setResponse(const char **argv, int argc,
    OPS_Stream &output)
{
    Response *theResponse = 0;

    output.tag("ElementOutput");
    output.attr("eleType","EETwoNodeLink");
    output.attr("eleTag",this->getTag());
    output.attr("node1",connectedExternalNodes[0]);
    output.attr("node2",connectedExternalNodes[1]);

    char outputData[10];

    // global forces
    if (strcmp(argv[0],"force") == 0 || strcmp(argv[0],"forces") == 0 ||
        strcmp(argv[0],"globalForce") == 0 || strcmp(argv[0],"globalForces") == 0)
    {
        for (int i=0; i<numDOF; i++)  {
            sprintf(outputData,"P%d",i+1);
            output.tag("ResponseType",outputData);
        }
        theResponse = new ElementResponse(this, 1, *theVector);
    }
    // local forces
    else if (strcmp(argv[0],"localForce") == 0 || strcmp(argv[0],"localForces") == 0)
    {
        for (int i=0; i<numDOF; i++)  {
            sprintf(outputData,"p%d",i+1);
            output.tag("ResponseType",outputData);
        }
        theResponse = new ElementResponse(this, 2, *theVector);
    }
    // basic forces
    else if (strcmp(argv[0],"basicForce") == 0 || strcmp(argv[0],"basicForces") == 0)
    {
        for (int i=0; i<numDir; i++)  {
            sprintf(outputData,"q%d",i+1);
            output.tag("ResponseType",outputData);
        }
        theResponse = new ElementResponse(this, 3, Vector(numDir));
    }
    // target local displacements
    else if (strcmp(argv[0],"localDisplacement") == 0 ||
        strcmp(argv[0],"localDisplacements") == 0)
    {
        for (int i=0; i<numDOF; i++)  {
            sprintf(outputData,"dl%d",i+1);
            output.tag("ResponseType",outputData);
        }
        theResponse = new ElementResponse(this, 4, Vector(numDir));
    }
    // target basic displacements
    else if (strcmp(argv[0],"deformation") == 0 || strcmp(argv[0],"deformations") == 0 || 
        strcmp(argv[0],"basicDeformation") == 0 || strcmp(argv[0],"basicDeformations") == 0 ||
        strcmp(argv[0],"targetDisplacement") == 0 || strcmp(argv[0],"targetDisplacements") == 0)
    {
        for (int i=0; i<numDir; i++)  {
            sprintf(outputData,"db%d",i+1);
            output.tag("ResponseType",outputData);
        }
        theResponse = new ElementResponse(this, 5, Vector(numDir));
    }
    // target basic velocities
    else if (strcmp(argv[0],"targetVelocity") == 0 || 
        strcmp(argv[0],"targetVelocities") == 0)
    {
        for (int i=0; i<numDir; i++)  {
            sprintf(outputData,"vb%d",i+1);
            output.tag("ResponseType",outputData);
        }
        theResponse = new ElementResponse(this, 6, Vector(numDir));
    }
    // target basic accelerations
    else if (strcmp(argv[0],"targetAcceleration") == 0 || 
        strcmp(argv[0],"targetAccelerations") == 0)
    {
        for (int i=0; i<numDir; i++)  {
            sprintf(outputData,"ab%d",i+1);
            output.tag("ResponseType",outputData);
        }
        theResponse = new ElementResponse(this, 7, Vector(numDir));
    }
    // measured basic displacements
    else if (strcmp(argv[0],"measuredDisplacement") == 0 || 
        strcmp(argv[0],"measuredDisplacements") == 0)
    {
        for (int i=0; i<numDir; i++)  {
            sprintf(outputData,"dbm%d",i+1);
            output.tag("ResponseType",outputData);
        }
        theResponse = new ElementResponse(this, 8, Vector(numDir));
    }
    // measured basic velocities
    else if (strcmp(argv[0],"measuredVelocity") == 0 || 
        strcmp(argv[0],"measuredVelocities") == 0)
    {
        for (int i=0; i<numDir; i++)  {
            sprintf(outputData,"vbm%d",i+1);
            output.tag("ResponseType",outputData);
        }
        theResponse = new ElementResponse(this, 9, Vector(numDir));
    }
    // measured basic accelerations
    else if (strcmp(argv[0],"measuredAcceleration") == 0 || 
        strcmp(argv[0],"measuredAccelerations") == 0)
    {
        for (int i=0; i<numDir; i++)  {
            sprintf(outputData,"abm%d",i+1);
            output.tag("ResponseType",outputData);
        }
        theResponse = new ElementResponse(this, 10, Vector(numDir));
    }
    // basic deformations and basic forces
    else if (strcmp(argv[0],"defoANDforce") == 0 || strcmp(argv[0],"deformationANDforce") == 0 ||
        strcmp(argv[0],"deformationsANDforces") == 0)
    {
        int i;
        for (i=0; i<numDir; i++)  {
            sprintf(outputData,"db%d",i+1);
            output.tag("ResponseType",outputData);
        }
        for (i=0; i<numDir; i++)  {
            sprintf(outputData,"q%d",i+1);
            output.tag("ResponseType",outputData);
        }
        theResponse = new ElementResponse(this, 11, Vector(numDir*2));
    }

    output.endTag(); // ElementOutput

    return theResponse;
}


int EETwoNodeLink::getResponse(int responseID, Information &eleInfo)
{
    static Vector defoAndForce(numDir*2);
    
    switch (responseID)  {
    case 1:  // global forces
        return eleInfo.setVector(this->getResistingForce());
        
    case 2:  // local forces
        theVector->Zero();
        // determine resisting forces in local system
        (*theVector) = Tlb^(*qMeas);
        // add P-Delta effects to local forces
        if (Mratio.Size() == 4)
            this->addPDeltaForces(*theVector);
        
        return eleInfo.setVector(*theVector);
        
    case 3:  // basic forces
        return eleInfo.setVector(*qMeas);
        
	case 4:  // target local displacements
        return eleInfo.setVector(dl);
        
    case 5:  // target basic displacements
        return eleInfo.setVector(dbTarg);
        
    case 6:  // target basic velocities
        return eleInfo.setVector(vbTarg);
        
    case 7:  // target basic accelerations
        return eleInfo.setVector(abTarg);
        
    case 8:  // measured basic displacements
        return eleInfo.setVector(this->getBasicDisp());
        
    case 9:  // measured basic velocities
        return eleInfo.setVector(this->getBasicVel());
        
    case 10:  // measured basic accelerations
        return eleInfo.setVector(this->getBasicAccel());
        
    case 11:  // basic deformations and basic forces
        defoAndForce.Zero();
        defoAndForce.Assemble(dbTarg,0);
        defoAndForce.Assemble(*qMeas,numDir);
        
        return eleInfo.setVector(defoAndForce);
        
    default:
        return 0;
    }
}


// set up the transformation matrix for orientation
void EETwoNodeLink::setUp()
{
    const Vector &end1Crd = theNodes[0]->getCrds();
    const Vector &end2Crd = theNodes[1]->getCrds();	
    Vector xp = end2Crd - end1Crd;
    L = xp.Norm();
    
    if (L > DBL_EPSILON)  {
        if (x.Size() == 0)  {
            x.resize(3);
            x.Zero();
            x(0) = xp(0);
            if (xp.Size() > 1)
                x(1) = xp(1);
            if (xp.Size() > 2)
                x(2) = xp(2);
        } else  {
            opserr << "WARNING EETwoNodeLink::setUp() - "
                << "element: " << this->getTag() << endln
                << "ignoring nodes and using specified "
                << "local x vector to determine orientation\n";
        }
    }
    // check that vectors for orientation are of correct size
    if (x.Size() != 3 || y.Size() != 3)  {
        opserr << "EETwoNodeLink::setUp() - "
            << "element: " << this->getTag() << endln
            << "incorrect dimension of orientation vectors\n";
        exit(-1);
    }
    
    // establish orientation of element for the tranformation matrix
    // z = x cross yp
    Vector z(3);
    z(0) = x(1)*y(2) - x(2)*y(1);
    z(1) = x(2)*y(0) - x(0)*y(2);
    z(2) = x(0)*y(1) - x(1)*y(0);
    
    // y = z cross x
    y(0) = z(1)*x(2) - z(2)*x(1);
    y(1) = z(2)*x(0) - z(0)*x(2);
    y(2) = z(0)*x(1) - z(1)*x(0);
    
    // compute length(norm) of vectors
    double xn = x.Norm();
    double yn = y.Norm();
    double zn = z.Norm();
    
    // check valid x and y vectors, i.e. not parallel and of zero length
    if (xn == 0 || yn == 0 || zn == 0)  {
        opserr << "EETwoNodeLink::setUp() - "
            << "element: " << this->getTag() << endln
            << "invalid orientation vectors\n";
        exit(-1);
    }
    
    // create transformation matrix of direction cosines
    for (int i=0; i<3; i++)  {
        trans(0,i) = x(i)/xn;
        trans(1,i) = y(i)/yn;
        trans(2,i) = z(i)/zn;
    }
}


// set transformation matrix from global to local system
void EETwoNodeLink::setTranGlobalLocal()
{
    // resize transformation matrix and zero it
    Tgl.resize(numDOF,numDOF);
    Tgl.Zero();
    
    for (int i=0; i<numDOF/2; i++)  {
        
        // switch on dimensionality of element
        switch (elemType)  {
        case D1N2:
            Tgl(i,0) = Tgl(i+1,1) = trans(i,0);
            break;
        case D2N4:
            Tgl(i,0) = Tgl(i+2,2) = trans(i,0);  
            Tgl(i,1) = Tgl(i+2,3) = trans(i,1);
            break;
        case D2N6: 
            if (i<2)  {
                Tgl(i,0) = Tgl(i+3,3) = trans(i,0);  
                Tgl(i,1) = Tgl(i+3,4) = trans(i,1);
            } else  {
                Tgl(i,2) = Tgl(i+3,5) = trans(i,2);
            }
            break;
        case D3N6:
            Tgl(i,0) = Tgl(i+3,3) = trans(i,0);  
            Tgl(i,1) = Tgl(i+3,4) = trans(i,1);
            Tgl(i,2) = Tgl(i+3,5) = trans(i,2);
            break;
        case D3N12:
            if (i < 3)  {
                Tgl(i,0) = Tgl(i+6,6)  = trans(i,0);
                Tgl(i,1) = Tgl(i+6,7)  = trans(i,1);
                Tgl(i,2) = Tgl(i+6,8)  = trans(i,2);
            } else  {
                Tgl(i,3) = Tgl(i+6,9)  = trans(i,0);
                Tgl(i,4) = Tgl(i+6,10) = trans(i,1);
                Tgl(i,5) = Tgl(i+6,11) = trans(i,2);
            }
            break;
        }
    }
}


// set transformation matrix from local to basic system
void EETwoNodeLink::setTranLocalBasic()
{
    // resize transformation matrix and zero it
    Tlb.resize(numDir,numDOF);
    Tlb.Zero();
    
    for (int i=0; i<numDir; i++)  {
        
        int dirID = (*dir)(i);     // direction 0 to 5;
        Tlb(i,dirID) = -1.0;
        Tlb(i,dirID+numDOF/2) = 1.0;
        
        // switch on dimensionality of element
        switch (elemType)  {
        case D2N6:
            if (dirID == 1)
                Tlb(i,2) = Tlb(i,5) = -0.5*L;
            break;
        case D3N12:
            if (dirID == 1)
                Tlb(i,5) = Tlb(i,11) = -0.5*L;
            else if (dirID == 2)
                Tlb(i,4) = Tlb(i,10) = 0.5*L;
        }
    }
}


void EETwoNodeLink::addPDeltaForces(Vector &pLocal)
{
    int dirID;
    double N = 0.0;
    double deltal1 = 0.0;
    double deltal2 = 0.0;
    
    for (int i=0; i<numDir; i++)  {
        dirID = (*dir)(i);  // direction 0 to 5;
        
        // get axial force and local disp differences
        if (dirID == 0)
            N = (*qMeas)(i);
        else if (dirID == 1)
            deltal1 = dl(1+numDOF/2) - dl(1);
        else if (dirID == 2)
            deltal2 = dl(2+numDOF/2) - dl(2);
    }
    
    if (N != 0.0 && (deltal1 != 0.0 || deltal2 != 0.0))  {
        for (int i=0; i<numDir; i++)  {
            dirID = (*dir)(i);  // direction 0 to 5;
            
            // switch on dimensionality of element
            switch (elemType)  {
            case D2N4:
                if (dirID == 1)  {
                    double VpDelta = N*deltal1/L;
                    VpDelta *= 1.0-Mratio(2)-Mratio(3);
                    pLocal(1) -= VpDelta;
                    pLocal(3) += VpDelta;
                }
                break;
            case D2N6: 
                if (dirID == 1)  {
                    double VpDelta = N*deltal1/L;
                    VpDelta *= 1.0-Mratio(2)-Mratio(3);
                    pLocal(1) -= VpDelta;
                    pLocal(4) += VpDelta;
                }
                else if (dirID == 2)  {
                    double MpDelta = N*deltal1;
                    pLocal(2) += Mratio(2)*MpDelta;
                    pLocal(5) += Mratio(3)*MpDelta;
                }
                break;
            case D3N6:
                if (dirID == 1)  {
                    double VpDelta = N*deltal1/L;
                    VpDelta *= 1.0-Mratio(2)-Mratio(3);
                    pLocal(1) -= VpDelta;
                    pLocal(4) += VpDelta;
                }
                else if (dirID == 2)  {
                    double VpDelta = N*deltal2/L;
                    VpDelta *= 1.0-Mratio(0)-Mratio(1);
                    pLocal(2) -= VpDelta;
                    pLocal(5) += VpDelta;
                }
                break;
            case D3N12:
                if (dirID == 1)  {
                    double VpDelta = N*deltal1/L;
                    VpDelta *= 1.0-Mratio(2)-Mratio(3);
                    pLocal(1) -= VpDelta;
                    pLocal(7) += VpDelta;
                }
                else if (dirID == 2)  {
                    double VpDelta = N*deltal2/L;
                    VpDelta *= 1.0-Mratio(0)-Mratio(1);
                    pLocal(2) -= VpDelta;
                    pLocal(8) += VpDelta;
                }
                else if (dirID == 4)  {
                    double MpDelta = N*deltal2;
                    pLocal(4) -= Mratio(0)*MpDelta;
                    pLocal(10) -= Mratio(1)*MpDelta;
                }
                else if (dirID == 5)  {
                    double MpDelta = N*deltal1;
                    pLocal(5) += Mratio(2)*MpDelta;
                    pLocal(11) += Mratio(3)*MpDelta;
                }
                break;
            }
        }
    }
}


void EETwoNodeLink::addPDeltaStiff(Matrix &kLocal)
{
    int dirID;
    double N = 0.0;
    
    // get axial force
    for (int i=0; i<numDir; i++)  {
        if ((*dir)(i) == 0)
            N = (*qMeas)(i);
    }
    
    if (N != 0.0)  {
        for (int i=0; i<numDir; i++)  {
            dirID = (*dir)(i);  // direction 0 to 5;
            
            // switch on dimensionality of element
            switch (elemType)  {
            case D2N4:
                if (dirID == 1)  {
                    double NoverL = N/L;
                    NoverL *= 1.0-Mratio(2)-Mratio(3);
                    kLocal(1,1) += NoverL;
                    kLocal(1,3) -= NoverL;
                    kLocal(3,1) -= NoverL;
                    kLocal(3,3) += NoverL;
                }
                break;
            case D2N6: 
                if (dirID == 1)  {
                    double NoverL = N/L;
                    NoverL *= 1.0-Mratio(2)-Mratio(3);
                    kLocal(1,1) += NoverL;
                    kLocal(1,4) -= NoverL;
                    kLocal(4,1) -= NoverL;
                    kLocal(4,4) += NoverL;
                }
                else if (dirID == 2)  {
                    kLocal(2,1) -= Mratio(2)*N;
                    kLocal(2,4) += Mratio(2)*N;
                    kLocal(5,1) -= Mratio(3)*N;
                    kLocal(5,4) += Mratio(3)*N;
                }
                break;
            case D3N6:
                if (dirID == 1)  {
                    double NoverL = N/L;
                    NoverL *= 1.0-Mratio(2)-Mratio(3);
                    kLocal(1,1) += NoverL;
                    kLocal(1,4) -= NoverL;
                    kLocal(4,1) -= NoverL;
                    kLocal(4,4) += NoverL;
                }
                else if (dirID == 2)  {
                    double NoverL = N/L;
                    NoverL *= 1.0-Mratio(0)-Mratio(1);
                    kLocal(2,2) += NoverL;
                    kLocal(2,5) -= NoverL;
                    kLocal(5,2) -= NoverL;
                    kLocal(5,5) += NoverL;
                }
                break;
            case D3N12:
                if (dirID == 1)  {
                    double NoverL = N/L;
                    NoverL *= 1.0-Mratio(2)-Mratio(3);
                    kLocal(1,1) += NoverL;
                    kLocal(1,7) -= NoverL;
                    kLocal(7,1) -= NoverL;
                    kLocal(7,7) += NoverL;
                }
                else if (dirID == 2)  {
                    double NoverL = N/L;
                    NoverL *= 1.0-Mratio(0)-Mratio(1);
                    kLocal(2,2) += NoverL;
                    kLocal(2,8) -= NoverL;
                    kLocal(8,2) -= NoverL;
                    kLocal(8,8) += NoverL;
                }
                else if (dirID == 4)  {
                    kLocal(4,2) += Mratio(0)*N;
                    kLocal(4,8) -= Mratio(0)*N;
                    kLocal(10,2) += Mratio(1)*N;
                    kLocal(10,8) -= Mratio(1)*N;
                }
                else if (dirID == 5)  {
                    kLocal(5,1) -= Mratio(2)*N;
                    kLocal(5,7) += Mratio(2)*N;
                    kLocal(11,1) -= Mratio(3)*N;
                    kLocal(11,7) += Mratio(3)*N;
                }
                break;
            }
        }
    }
}


void EETwoNodeLink::applyIMod()
{    
    // get measured displacements
    if (theSite != 0)  {
        (*dbMeas) = theSite->getDisp();
    }
    else  {
        sData[0] = OF_RemoteTest_getDisp;
        theChannel->sendVector(0, 0, *sendData, 0);
        theChannel->recvVector(0, 0, *recvData, 0);
    }
    
    // correct for displacement control errors using I-Modification
    (*qMeas) -= kbInit*((*dbMeas) - (*db));
}
