#include <stdio.h>
#include <stdlib.h>



int cmpfunc (const void * a, const void * b)
{
   return ( *(int*)a - *(int*)b );
}

int main()
{
   int* values;
   int n;
   values = malloc(5*sizeof(int));
   values[0] = 1;
   values[1] = 0;
   values[2] = 9;
   values[3] = 5;
   values[4] = 7;

   printf("Before sorting the list is: \n");
   for( n = 0 ; n < 5; n++ ) {
      printf("%d ", values[n]);
   }

   qsort(&values[2], 3, sizeof(int), cmpfunc);

   printf("\nAfter sorting the list is: \n");
   for( n = 0 ; n < 5; n++ ) {
      printf("%d ", values[n]);
   }
  
  return(0);
}