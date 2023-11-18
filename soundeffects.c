#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>


// See http://soundfile.sapp.org/doc/WaveFormat/


struct wav {
   char riff[5];  // chunk ID (just "RIFF")
   int size;      // chunk size
   char ftype[5]; // chunk format (just "WAVE")
   char fmt[5]; // subchunk ID (just "fmt ")
   int subsize; // size of rest of subchunk, 16 for PCM
   short audioformat; // if value other than 1, some form of compression
   short nchnl; // mono = 1, sterio = 2
   int smprate; // the sample rate (8000, 44100, etc.)
   int byterate; // = SampleRate * NumChannels * BitsPerSample/8
   short align; // = NumChannels * BitsPerSample/8; number of bytes for 1 sample incld. all channels
   short bitspersample; // 8 bits = 8, 16 bits = 16, etc.
   char datastart[5]; // Subchunk2 ID (just "data")
   int nbytes; // the number of bytes in the data; also size of the rest of the subchunk following this number
} ;


#define WAVHDRSIZE (44) // the number of bytes in .wav header
#define BUFSIZE (4096) // buffer size
#define PCM (1) // if 1 not compressed, if any other number that it's compressed

/**
* Applies a sound effect to WAV file that causes sound balance to shift one channel to another
* Rate specified by 'freq' and the max volume is indicated by 'MAX'
*
*/
void soundeffect(FILE *ifp, FILE *ofp, double freq, float MAX, struct wav *hdr) {
    if (ifp == NULL || ofp == NULL || hdr == NULL) {
        return;
    }

    char* header_buffer = (char*)malloc(sizeof(char) * WAVHDRSIZE);
    if (header_buffer == NULL) {
        fprintf(stderr, "MEM ERR: failed to allocate memory\n");
        exit(1);
    }

    short* buffer = (short*)malloc(sizeof(short) * BUFSIZE / 2); // BUFSIZE / 2 since each sample is 2 bytes
    if (buffer == NULL) {
        fprintf(stderr, "MEM ERR: failed to allocate memory\n");
        exit(1);
    }

    double amp_mod = 0;
    int bytepersample = hdr->bitspersample / 8;

    fseek(ifp, 0, SEEK_SET);
    fseek(ofp, 0, SEEK_SET);
    fread(header_buffer, 1, WAVHDRSIZE, ifp);
    fwrite(header_buffer, 1, WAVHDRSIZE, ofp);

    int bytesRead;

    while ((bytesRead = fread(buffer, 1, BUFSIZE, ifp)) > 0) {
        // Iterate through the buffer and apply the sound effect
        for (int i = 0; i < bytesRead; i += bytepersample * hdr->nchnl) {
            if (amp_mod > 1 || amp_mod < 0) {
                freq = -freq;
            }
            amp_mod += freq;

            for (int channel = 0; channel < hdr->nchnl; ++channel) {
                short* currentSample = &buffer[i + channel * bytepersample / 2];

                // Modify the current sample for the current channel
                *currentSample = (short)(*currentSample * amp_mod * MAX);
            }
        }

        // Write the modified buffer to the output file
        fwrite(buffer, 1, bytesRead, ofp);
    }

    free(header_buffer);
    free(buffer);
}



struct wav * readwavhdr(FILE *ifp) {
    struct wav *hdr;

    hdr = (struct wav *) malloc(WAVHDRSIZE);

    if (hdr == NULL) {
        fprintf(stderr, "MEM ERR: failed to allocate memory\n");
        free(hdr);
        exit(1);
    }

    // Starts reader from top of file
    fseek(ifp,0,SEEK_SET);
    fread(hdr->riff, 4, 1, ifp);
    if (strcmp(hdr->riff, "RIFF") != 0) {
        printf("First 4 bytes should be \"RIFF\", are \"%s\"\n", hdr->riff);
        free(hdr);
        exit(1);
    }
    fread(&hdr->size, 4, 1, ifp);
    fread(hdr->ftype, 4, 1, ifp);
    fread(hdr->fmt, 4, 1, ifp);
    fread(&hdr->subsize, 4, 1, ifp);
    fread(&hdr->audioformat, 2, 1, ifp);
    fread(&hdr->nchnl, 2, 1, ifp);
    fread(&hdr->smprate, 4, 1, ifp);
    fread(&hdr->byterate, 4, 1, ifp);
    fread(&hdr->align, 2, 1, ifp);
    fread(&hdr->bitspersample, 2, 1, ifp);
    fread(hdr->datastart, 4, 1, ifp);
    fread(&hdr->nbytes, 4, 1, ifp);

    return hdr;
}


/**
* Print WAV header info.
*/
void printwavhdr(struct wav *hdr) {
   if (hdr == NULL) return;

   printf("riff\t\t%s\n", hdr->riff);
   printf("size\t\t%d\n", hdr->size);
   printf("nchnl\t\t%d\n", hdr->nchnl);
   printf("audioformat\t%d\n", hdr->audioformat);
   printf("smprate\t\t%d\n", hdr->smprate);
   printf("byterate\t%d\n", hdr->byterate);
   printf("bitspersample\t%d\n", hdr->bitspersample);
   printf("datastart\t%s\n", hdr->datastart);
   printf("nbytes\t\t%d\n", hdr->nbytes);
   }


/**
* Read input, apply effect, write output.
*/
int main(int argc, char *argv[]) {
    FILE* in_file; // the input file.wav
    struct wav *hdr;

    if (argc != 3) {
        goto ERR;
        exit(1);
    }

    in_file = fopen(argv[1], "rb"); // Opens for reading

    if (in_file == NULL) {
        fclose(in_file);
        goto ERR2;
        exit(1);
    }


    hdr = readwavhdr(in_file);
    printwavhdr(hdr);

    if (hdr->nchnl == 2 && hdr->audioformat == 1) {
        FILE* out_file; // write only file
        out_file = fopen(argv[2], "wb"); // Opens for writing
        if (out_file == NULL) {
            fclose(out_file);
            goto ERR2;
            exit(1);
        }   

        // 0.05 is the roof, gets wonky after that
        // third param is frequency measured in hertz
        // period = 1 / freq
        soundeffect(in_file, out_file, 0.0005, 0.9, hdr);
        fclose(out_file);

    } else {
        fclose(in_file);
        free(hdr);
        goto ERR3;
        exit(1);
    }


    printf("Closing file...\n");
    fclose(in_file);

    free(hdr);
    hdr = NULL;
    return(0); // successful completion


    ERR: fprintf(stderr,"USAGE: %s input.wav output.wav\n",argv[0]); // error!
    ERR2: fprintf(stderr, "FILE NOT FOUND: unable to open file %s\n", argv[1]); // file not found error
    ERR3: fprintf(stderr, "WRONG TYPE: .wav file must have 2 channels, be PCM format, and not be compressed\n"); // file type not correct


}