/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

void transpose_32(int M, int N, int A[N][M], int B[M][N]) {
    int block_size = 8;
    for (int i = 0; i < N; i += block_size) {
        for (int j = 0; j < M; j += block_size) {
            for (int k = 0; k < block_size; k += 1) {
                int temp0 = A[i + k][j];
                int temp1 = A[i + k][j + 1];
                int temp2 = A[i + k][j + 2];
                int temp3 = A[i + k][j + 3];
                int temp4 = A[i + k][j + 4];
                int temp5 = A[i + k][j + 5];
                int temp6 = A[i + k][j + 6];
                int temp7 = A[i + k][j + 7];

                B[j][i + k] = temp0;
                B[j + 1][i + k] = temp1;
                B[j + 2][i + k] = temp2;
                B[j + 3][i + k] = temp3;
                B[j + 4][i + k] = temp4;
                B[j + 5][i + k] = temp5;
                B[j + 6][i + k] = temp6;
                B[j + 7][i + k] = temp7;
            }
        }
    }
}

void transpose_64(int M, int N, int A[N][M], int B[M][N]) {
    int temp0, temp1, temp2, temp3, temp4, temp5, temp6, temp7;
    for (int i = 0; i < N; i += 8) {
        for (int j = 0; j < M; j += 8) {
            for (int k = 0; k < 4; k += 1) {
                temp0 = A[i + k][j];
                temp1 = A[i + k][j + 1];
                temp2 = A[i + k][j + 2];
                temp3 = A[i + k][j + 3];
                temp4 = A[i + k][j + 4];
                temp5 = A[i + k][j + 5];
                temp6 = A[i + k][j + 6];
                temp7 = A[i + k][j + 7];

                B[j][i + k] = temp0;
                B[j + 1][i + k] = temp1;
                B[j + 2][i + k] = temp2;
                B[j + 3][i + k] = temp3;
                B[j][i + k + 4] = temp4;
                B[j + 1][i + k + 4] = temp5;
                B[j + 2][i + k + 4] = temp6;
                B[j + 3][i + k + 4] = temp7;
            }

            for (int k = 0; k < 4; k += 1) {
                temp0 = B[j + k][i + 4];
                temp1 = B[j + k][i + 5];
                temp2 = B[j + k][i + 6];
                temp3 = B[j + k][i + 7];
                temp4 = A[i + 4][j + k];
                temp5 = A[i + 5][j + k];
                temp6 = A[i + 6][j + k];
                temp7 = A[i + 7][j + k];

                B[j + k][i + 4] = temp4;
                B[j + k][i + 5] = temp5;
                B[j + k][i + 6] = temp6;
                B[j + k][i + 7] = temp7;
                B[j + k + 4][i] = temp0;
                B[j + k + 4][i + 1] = temp1;
                B[j + k + 4][i + 2] = temp2;
                B[j + k + 4][i + 3] = temp3;
            }

            for (int k = 4; k < 8; k += 1) {
                temp0 = A[i + k][j + 4];
                temp1 = A[i + k][j + 5];
                temp2 = A[i + k][j + 6];
                temp3 = A[i + k][j + 7];
                B[j + 4][i + k] = temp0;
                B[j + 5][i + k] = temp1;
                B[j + 6][i + k] = temp2;
                B[j + 7][i + k] = temp3;
            }
        }
    }
}

void transpose_61_67(int M, int N, int A[N][M], int B[M][N]) {
    int temp0, temp1, temp2, temp3, temp4, temp5, temp6, temp7;
    for (int i = 0; i < 64; i += 8) {
        for (int j = 0; j < 56; j += 8) {
            for (int k = 0; k < 8; k += 1) {
                temp0 = A[i + k][j];
                temp1 = A[i + k][j + 1];
                temp2 = A[i + k][j + 2];
                temp3 = A[i + k][j + 3];
                temp4 = A[i + k][j + 4];
                temp5 = A[i + k][j + 5];
                temp6 = A[i + k][j + 6];
                temp7 = A[i + k][j + 7];

                B[j][i + k] = temp0;
                B[j + 1][i + k] = temp1;
                B[j + 2][i + k] = temp2;
                B[j + 3][i + k] = temp3;
                B[j + 4][i + k] = temp4;
                B[j + 5][i + k] = temp5;
                B[j + 6][i + k] = temp6;
                B[j + 7][i + k] = temp7;
            }
        }
    }
    for (int i = 0; i < 64; i += 4) {
        int j = 56;
        for (int k = 0; k < 4; k += 1) {
            temp0 = A[i + k][j];
            temp1 = A[i + k][j + 1];
            temp2 = A[i + k][j + 2];
            temp3 = A[i + k][j + 3];
            temp4 = A[i + k][j + 4];
            B[j][i + k] = temp0;
            B[j + 1][i + k] = temp1;
            B[j + 2][i + k] = temp2;
            B[j + 3][i + k] = temp3;
            B[j + 4][i + k] = temp4;
        }
    }
    for (int i = 64; i < N; i += 4) {
        for (int j = 0; j < 56; j += 4) {
            for (int k = 0; k < 3; k += 1) {
                temp0 = A[i + k][j];
                temp1 = A[i + k][j + 1];
                temp2 = A[i + k][j + 2];
                temp3 = A[i + k][j + 3];
                B[j][i + k] = temp0;
                B[j + 1][i + k] = temp1;
                B[j + 2][i + k] = temp2;
                B[j + 3][i + k] = temp3;
            }
        }
    }
    for (int i = 64; i < N; i += 1) {
        for (int j = 56; j < M; j += 1) {
            temp0 = A[i][j];
            B[j][i] = temp0;
        }
    }
}

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N]) {
    if (M == 32 && N == 32) {
        transpose_32(32, 32, A, B);
        return;
    } else if (M == 64 && N == 64) {
        transpose_64(64, 64, A, B);
        return;
    } else if (M == 61 && N == 67) {
        transpose_61_67(61, 67, A, B);
        return;
    }
    int block_size = 32 / sizeof(int);
    for (int i = 0; i < N; i += block_size) {
        for (int j = 0; j < M; j += block_size) {
            for (int x = 0; x < block_size; x += 1) {
                for (int y = 0; y < block_size; y += 1) {
                    int row = i + x;
                    int col = j + y;
                    if (row >= N || col >= M) {
                        continue;
                    }
                    B[col][row] = A[row][col];
                }
            }
        }
    }
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N]) {
    int i, j, tmp;
    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions() {
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N]) {
    int i, j;
    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

