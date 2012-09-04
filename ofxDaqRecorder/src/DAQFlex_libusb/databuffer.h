#ifndef DATABUFFER_H
#define DATABUFFER_H

class dataBuffer
{
    public:
        dataBuffer(int points)
        {
            if(points % 32)
                throw MCC_ERR_INVALID_BUFFER_SIZE;
            numPoints = points;
            data = new unsigned short[points];
			currReadIndex = 0;
        };

        ~dataBuffer()
        {
            delete data;
        }

        unsigned int getNumPoints()
        {
            return numPoints;
        }

        int sizeOf(){
            return 2;
        }

        unsigned int currIndex;
        unsigned int currCount;
		unsigned int currReadIndex;
        unsigned short* data;
    private:
        unsigned int numPoints;
};

#endif
