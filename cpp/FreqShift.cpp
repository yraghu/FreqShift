/**
* Copyright (C) 2015 Axios, Inc.
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "FreqShift.h"

PREPARE_LOGGING(FreqShift_i)

FreqShift_i::FreqShift_i(const char *uuid, const char *label) :
    FreqShift_base(uuid, label)
{
	sampleRate = 0;
	firstTime = true;
}

FreqShift_i::~FreqShift_i()
{
}

/***********************************************************************************************

    Basic functionality:

        The service function is called by the serviceThread object (of type ProcessThread).
        This call happens immediately after the previous call if the return value for
        the previous call was NORMAL.
        If the return value for the previous call was NOOP, then the serviceThread waits
        an amount of time defined in the serviceThread's constructor.
        
    SRI:
        To create a StreamSRI object, use the following code:
                std::string stream_id = "testStream";
                BULKIO::StreamSRI sri = bulkio::sri::create(stream_id);

	Time:
	    To create a PrecisionUTCTime object, use the following code:
                BULKIO::PrecisionUTCTime tstamp = bulkio::time::utils::now();

        
    Ports:

        Data is passed to the serviceFunction through the getPacket call (BULKIO only).
        The dataTransfer class is a port-specific class, so each port implementing the
        BULKIO interface will have its own type-specific dataTransfer.

        The argument to the getPacket function is a floating point number that specifies
        the time to wait in seconds. A zero value is non-blocking. A negative value
        is blocking.  Constants have been defined for these values, bulkio::Const::BLOCKING and
        bulkio::Const::NON_BLOCKING.

        Each received dataTransfer is owned by serviceFunction and *MUST* be
        explicitly deallocated.

        To send data using a BULKIO interface, a convenience interface has been added 
        that takes a std::vector as the data input

        NOTE: If you have a BULKIO dataSDDS or dataVITA49  port, you must manually call 
              "port->updateStats()" to update the port statistics when appropriate.

        Example:
            // this example assumes that the component has two ports:
            //  A provides (input) port of type bulkio::InShortPort called short_in
            //  A uses (output) port of type bulkio::OutFloatPort called float_out
            // The mapping between the port and the class is found
            // in the component base class header file

            bulkio::InShortPort::dataTransfer *tmp = short_in->getPacket(bulkio::Const::BLOCKING);
            if (not tmp) { // No data is available
                return NOOP;
            }

            std::vector<float> outputData;
            outputData.resize(tmp->dataBuffer.size());
            for (unsigned int i=0; i<tmp->dataBuffer.size(); i++) {
                outputData[i] = (float)tmp->dataBuffer[i];
            }

            // NOTE: You must make at least one valid pushSRI call
            if (tmp->sriChanged) {
                float_out->pushSRI(tmp->SRI);
            }
            float_out->pushPacket(outputData, tmp->T, tmp->EOS, tmp->streamID);

            delete tmp; // IMPORTANT: MUST RELEASE THE RECEIVED DATA BLOCK
            return NORMAL;

        If working with complex data (i.e., the "mode" on the SRI is set to
        true), the std::vector passed from/to BulkIO can be typecast to/from
        std::vector< std::complex<dataType> >.  For example, for short data:

            bulkio::InShortPort::dataTransfer *tmp = myInput->getPacket(bulkio::Const::BLOCKING);
            std::vector<std::complex<short> >* intermediate = (std::vector<std::complex<short> >*) &(tmp->dataBuffer);
            // do work here
            std::vector<short>* output = (std::vector<short>*) intermediate;
            myOutput->pushPacket(*output, tmp->T, tmp->EOS, tmp->streamID);

        Interactions with non-BULKIO ports are left up to the component developer's discretion

    Properties:
        
        Properties are accessed directly as member variables. For example, if the
        property name is "baudRate", it may be accessed within member functions as
        "baudRate". Unnamed properties are given a generated name of the form
        "prop_n", where "n" is the ordinal number of the property in the PRF file.
        Property types are mapped to the nearest C++ type, (e.g. "string" becomes
        "std::string"). All generated properties are declared in the base class
        (FreqShift_base).
    
        Simple sequence properties are mapped to "std::vector" of the simple type.
        Struct properties, if used, are mapped to C++ structs defined in the
        generated file "struct_props.h". Field names are taken from the name in
        the properties file; if no name is given, a generated name of the form
        "field_n" is used, where "n" is the ordinal number of the field.
        
        Example:
            // This example makes use of the following Properties:
            //  - A float value called scaleValue
            //  - A boolean called scaleInput
              
            if (scaleInput) {
                dataOut[i] = dataIn[i] * scaleValue;
            } else {
                dataOut[i] = dataIn[i];
            }
            
        Callback methods can be associated with a property so that the methods are
        called each time the property value changes.  This is done by calling 
        addPropertyChangeListener(<property name>, this, &FreqShift_i::<callback method>)
        in the constructor.

        Callback methods should take two arguments, both const pointers to the value
        type (e.g., "const float *"), and return void.

        Example:
            // This example makes use of the following Properties:
            //  - A float value called scaleValue
            
        //Add to FreqShift.cpp
        FreqShift_i::FreqShift_i(const char *uuid, const char *label) :
            FreqShift_base(uuid, label)
        {
            addPropertyChangeListener("scaleValue", this, &FreqShift_i::scaleChanged);
        }

        void FreqShift_i::scaleChanged(const float *oldValue, const float *newValue)
        {
            std::cout << "scaleValue changed from" << *oldValue << " to " << *newValue
                      << std::endl;
        }
            
        //Add to FreqShift.h
        void scaleChanged(const float* oldValue, const float* newValue);
        
        
************************************************************************************************/
int FreqShift_i::serviceFunction()
{
    LOG_DEBUG(FreqShift_i, "serviceFunction() example log message");
    bulkio::InFloatPort::dataTransfer *tmp = float_in->getPacket(bulkio::Const::BLOCKING);
    if (not tmp) { // No data is available
    	return NOOP;
    }

    vector<float> *output;	//pointer to output data
    sampleRate = 1/tmp->SRI.xdelta;	//stores sample rate as a function of the inverse of time between samples
    vector<complex<float> > complex_vector;
    complex_vector.resize(tmp->dataBuffer.size());

    //Generates a vector which stores to the real and imaginary parts of a complex exponential
    //containing the desired amount by which the frequency is to be shifted
    for(unsigned int i=0;i<tmp->dataBuffer.size();i++)
    {
    	float theta = 2*M_PI*frequency_shift*i*tmp->SRI.xdelta;
    	float real_part = cos(theta);
    	float imag_part = sin(theta);
    	complex<float> complex_value(real_part, imag_part);
    	complex_vector[i] = complex_value;
    }

    //If signal is complex, takes the product of the respective data points of the input data vector
    //and complex_vector and stores result in the each element of data. Shifts the
    //frequency by frequency_shift Hz
    if(tmp->SRI.mode)
    {
    	vector<complex<float> > *input = (vector<complex<float> > *)&tmp->dataBuffer;
    	vectormultiply(*input, complex_vector, data);
    }

    //If signal is purely real, takes the product of the respective data points of the input data vector
    //and complex_vector and stores result in the each element of data. Shifts the
    //frequency by frequency_shift Hz
    else
    {
    	vector<float> *input = (vector<float> *)&tmp->dataBuffer;
    	vectormultiply(*input, complex_vector, data);
    }
    output = &data;

    //If this is the first time the service function is run, set mode equal to 1
    //for complex and push SRI. This only runs the first iteration, as the output data
    //will always be complex
    if(firstTime)
    {
        tmp->SRI.mode = 1;
    	float_out->pushSRI(tmp->SRI);
    	firstTime = false;
    }

    float_out->pushPacket(*output, tmp->T, tmp->EOS, tmp->streamID);

    delete tmp; // IMPORTANT: MUST RELEASE THE RECEIVED DATA BLOCK
    return NORMAL;
}

