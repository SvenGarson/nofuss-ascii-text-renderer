#include <stdio.h>
#include "..\include\nofuss_ascii_text_renderer.h"

int main(int argc, char * argv[])
{
   // Use library
   struct nfatr_instance nfatr;
   if (nfatr_instantiate(&nfatr, 1000, 256, 256))
   {
      printf("\nSuccess - Way to go!");
   }
   else
   {
      printf("\nFailure - Hmmm ...");
   }

   // To OS
   return 0;
}