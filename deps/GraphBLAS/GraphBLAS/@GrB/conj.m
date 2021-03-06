function C = conj (G)
%CONJ complex conjugate of a GraphBLAS matrix.
% Since all GraphBLAS matrices are currently real, conj (G) is just G.
% Complex support will be added in the future.

% SuiteSparse:GraphBLAS, Timothy A. Davis, (c) 2017-2019, All Rights Reserved.
% http://suitesparse.com   See GraphBLAS/Doc/License.txt for license.

C = G ;

