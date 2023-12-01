#include <stdio.h>
#include <stdint.h>

#define F (1 << 14)
#define INT_MAX ((1 << 31) - 1)
#define INT_MIN (-(1 << 31))

int int_to_fp(int n);
int fp_to_int_round(int x);
int fp_to_int(int x);
int add_fp(int x, int y);
int add_mixed(int x, int n);
int sub_fup(int x, int y);
int sub_mixed(int x, int y);
int mult_fp(int x, int y);
int mult_mixed(int x, int y);
int div_fp(int x, int y);
int div_mixed(int x, int n);

/* integer to fixed point */
int int_to_fp(int n)
{
    // 고려사항!
    // n을 어디까지 제한해야할까?
    return n * F;
}

/* fixed point to integer */
int fp_to_int(int x)
{
    return x / F;
}

/* fixed point to integer */
int fp_to_int_round(int x)
{
    if (x >= 0)
    {
        return ((x + F / 2) / F);
    }
    else
    {
        return ((x - F / 2) / F);
    }
}

/* Add between FP */
int add_fp(int x, int y)
{
    return x + y;
}

/* Add between FP and int */
int add_mixed(int x, int n)
{
    return x + (n * F);
}

/* Subtract between FP */
int sub_fup(int x, int y)
{
    return x - y;
}

/* Subtract between FP and int */
int sub_mixed(int x, int n)
{
    return x - (n * F);
}

/* Multiple between FP */
int mult_fp(int x, int y)
{
    return (int)(((int64_t)x) * y / F);
}

/* Multiple between FP and int */
int mult_mixed(int x, int n)
{
    return x * n;
}

/* Divide between FP */
int div_fp(int x, int y)
{
    return (int)(((int64_t)x) * F / y);
}

/* Divide between FP and int */
int div_mixed(int x, int n)
{
    return x / n;
}

// void printbits(int x)
// {
//     for (int i = 31; i >= 0; i--)
//     {
//         int bit = (x >> i) & 1;
//         printf("%d", bit);

//         if (i == 31 || i == 14)
//         {
//             printf(" | ");
//         }
//     }
//     printf("\n");
// }

// int main()
// {
//     int n = 1244;
//     printf("Here's n Value : %d\n", n);
//     printf("Here's n to FP Bit represention : ");
//     printbits(int_to_fp(n));
//     printf("Here's n to int bit represention : ");
//     printbits(fp_to_int_round(int_to_fp(n)));

//     printf("Here's div_mixed function result: ");
//     printbits(div_mixed(int_to_fp(30), 3));
//     printf("\n");
//     return 0;
// }