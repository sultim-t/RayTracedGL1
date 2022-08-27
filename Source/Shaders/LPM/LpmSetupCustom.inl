// @RTGL1 - BEGIN
// NOTE: this is a copy of LpmSetup with added parameter 'pContext' to 'LpmSetupOut',
// because with original function, we would need a global variable
// @RTGL1 - END



A_STATIC void LpmSetup(

// @RTGL1 - BEGIN
void * pContext,
// @RTGL1 - END

// Path control.
AP1 shoulder, // Use optional extra shoulderContrast tuning (set to false if shoulderContrast is 1.0).
// Prefab start, "LPM_CONFIG_".
AP1 con, // Use first RGB conversion matrix, if 'soft' then 'con' must be true also.
AP1 soft, // Use soft gamut mapping.
AP1 con2, // Use last RGB conversion matrix.
AP1 clip, // Use clipping in last conversion matrix.
AP1 scaleOnly, // Scale only for last conversion matrix (used for 709 HDR to scRGB).
// Gamut control, "LPM_COLORS_".
inAF2 xyRedW,inAF2 xyGreenW,inAF2 xyBlueW,inAF2 xyWhiteW, // Chroma coordinates for working color space.
inAF2 xyRedO,inAF2 xyGreenO,inAF2 xyBlueO,inAF2 xyWhiteO, // For the output color space.
inAF2 xyRedC,inAF2 xyGreenC,inAF2 xyBlueC,inAF2 xyWhiteC,AF1 scaleC, // For the output container color space (if con2).
// Prefab end.
AF1 softGap, // Range of 0 to a little over zero, controls how much feather region in out-of-gamut mapping, 0=clip.
// Tonemapping control.
AF1 hdrMax, // Maximum input value.
AF1 exposure, // Number of stops between 'hdrMax' and 18% mid-level on input. 
AF1 contrast, // Input range {0.0 (no extra contrast) to 1.0 (maximum contrast)}.
AF1 shoulderContrast, // Shoulder shaping, 1.0 = no change (fast path). 
inAF3 saturation, // A per channel adjustment, use <0 decrease, 0=no change, >0 increase.
inAF3 crosstalk){ // One channel must be 1.0, the rest can be <= 1.0 but not zero.
//-----------------------------------------------------------------------------------------------------------------------------
 // Contrast needs to be 1.0 based for no contrast.
 contrast+=AF1_(1.0);
 // Saturation is based on contrast.
 opAAddOneF3(saturation,saturation,contrast);
//-----------------------------------------------------------------------------------------------------------------------------
 // The 'softGap' must actually be above zero.
 softGap=AMaxF1(softGap,AF1_(1.0/1024.0));
//-----------------------------------------------------------------------------------------------------------------------------
 AF1 midIn=hdrMax*AF1_(0.18)*AExp2F1(-exposure);
 AF1 midOut=AF1_(0.18);
//-----------------------------------------------------------------------------------------------------------------------------
 varAF2(toneScaleBias);
 AF1 cs=contrast*shoulderContrast;
 AF1 z0=-APowF1(midIn,contrast);
 AF1 z1=APowF1(hdrMax,cs)*APowF1(midIn,contrast);
 AF1 z2=APowF1(hdrMax,contrast)*APowF1(midIn,cs)*midOut;
 AF1 z3=APowF1(hdrMax,cs)*midOut;
 AF1 z4=APowF1(midIn,cs)*midOut;
 toneScaleBias[0]=-((z0+(midOut*(z1-z2))*ARcpF1(z3-z4))*ARcpF1(z4));
//-----------------------------------------------------------------------------------------------------------------------------
 AF1 w0=APowF1(hdrMax,cs)*APowF1(midIn,contrast);
 AF1 w1=APowF1(hdrMax,contrast)*APowF1(midIn,cs)*midOut;
 AF1 w2=APowF1(hdrMax,cs)*midOut;
 AF1 w3=APowF1(midIn,cs)*midOut;
 toneScaleBias[1]=(w0-w1)*ARcpF1(w2-w3);
//----------------------------------------------------------------------------------------------------------------------------- 
 varAF3(lumaW);varAF3(rgbToXyzXW);varAF3(rgbToXyzYW);varAF3(rgbToXyzZW);
 LpmColRgbToXyz(rgbToXyzXW,rgbToXyzYW,rgbToXyzZW,xyRedW,xyGreenW,xyBlueW,xyWhiteW);
 // Use the Y vector of the matrix for the associated luma coef.
 // For safety, make sure the vector sums to 1.0.
 opAMulOneF3(lumaW,rgbToXyzYW,ARcpF1(rgbToXyzYW[0]+rgbToXyzYW[1]+rgbToXyzYW[2]));
//----------------------------------------------------------------------------------------------------------------------------- 
 // The 'lumaT' for crosstalk mapping is always based on the output color space, unless soft conversion is not used.
 varAF3(lumaT);varAF3(rgbToXyzXO);varAF3(rgbToXyzYO);varAF3(rgbToXyzZO);
 LpmColRgbToXyz(rgbToXyzXO,rgbToXyzYO,rgbToXyzZO,xyRedO,xyGreenO,xyBlueO,xyWhiteO);
 if(soft)opACpyF3(lumaT,rgbToXyzYO);else opACpyF3(lumaT,rgbToXyzYW);
 opAMulOneF3(lumaT,lumaT,ARcpF1(lumaT[0]+lumaT[1]+lumaT[2]));
 varAF3(rcpLumaT);opARcpF3(rcpLumaT,lumaT);
//-----------------------------------------------------------------------------------------------------------------------------
 varAF2(softGap2)=initAF2(0.0,0.0);
 if(soft){softGap2[0]=softGap;softGap2[1]=(AF1_(1.0)-softGap)*ARcpF1(softGap*AF1_(0.693147180559));}
//-----------------------------------------------------------------------------------------------------------------------------
 // First conversion is always working to output.
 varAF3(conR)=initAF3(0.0,0.0,0.0);
 varAF3(conG)=initAF3(0.0,0.0,0.0);
 varAF3(conB)=initAF3(0.0,0.0,0.0);
 if(con){varAF3(xyzToRgbRO);varAF3(xyzToRgbGO);varAF3(xyzToRgbBO);
  LpmMatInv3x3(xyzToRgbRO,xyzToRgbGO,xyzToRgbBO,rgbToXyzXO,rgbToXyzYO,rgbToXyzZO);
  LpmMatMul3x3(conR,conG,conB,xyzToRgbRO,xyzToRgbGO,xyzToRgbBO,rgbToXyzXW,rgbToXyzYW,rgbToXyzZW);}
//-----------------------------------------------------------------------------------------------------------------------------
 // The last conversion is always output to container.
 varAF3(con2R)=initAF3(0.0,0.0,0.0);
 varAF3(con2G)=initAF3(0.0,0.0,0.0);
 varAF3(con2B)=initAF3(0.0,0.0,0.0);
 if(con2){varAF3(rgbToXyzXC);varAF3(rgbToXyzYC);varAF3(rgbToXyzZC);
  LpmColRgbToXyz(rgbToXyzXC,rgbToXyzYC,rgbToXyzZC,xyRedC,xyGreenC,xyBlueC,xyWhiteC);
  varAF3(xyzToRgbRC);varAF3(xyzToRgbGC);varAF3(xyzToRgbBC);
  LpmMatInv3x3(xyzToRgbRC,xyzToRgbGC,xyzToRgbBC,rgbToXyzXC,rgbToXyzYC,rgbToXyzZC);
  LpmMatMul3x3(con2R,con2G,con2B,xyzToRgbRC,xyzToRgbGC,xyzToRgbBC,rgbToXyzXO,rgbToXyzYO,rgbToXyzZO);
  opAMulOneF3(con2R,con2R,scaleC);opAMulOneF3(con2G,con2G,scaleC);opAMulOneF3(con2B,con2B,scaleC);}
 if(scaleOnly)con2R[0]=scaleC;
//-----------------------------------------------------------------------------------------------------------------------------
 // Debug force 16-bit precision for the 32-bit inputs, only works on the GPU.
 #ifdef A_GPU
  #ifdef LPM_DEBUG_FORCE_16BIT_PRECISION
   saturation=AF3(AH3(saturation));
   contrast=AF1(AH1(contrast));
   toneScaleBias=AF2(AH2(toneScaleBias));
   lumaT=AF3(AH3(lumaT));
   crosstalk=AF3(AH3(crosstalk));
   rcpLumaT=AF3(AH3(rcpLumaT));
   con2R=AF3(AH3(con2R));
   con2G=AF3(AH3(con2G));
   con2B=AF3(AH3(con2B));
   shoulderContrast=AF1(AH1(shoulderContrast));
   lumaW=AF3(AH3(lumaW));
   softGap2=AF2(AH2(softGap2));
   conR=AF3(AH3(conR));
   conG=AF3(AH3(conG));
   conB=AF3(AH3(conB));
  #endif
 #endif
//-----------------------------------------------------------------------------------------------------------------------------
 // Pack into control block.
 varAU4(map0);
 map0[0]=AU1_AF1(saturation[0]);
 map0[1]=AU1_AF1(saturation[1]);
 map0[2]=AU1_AF1(saturation[2]);
 map0[3]=AU1_AF1(contrast);
// @RTGL1 - BEGIN
 LpmSetupOut(pContext, 0,map0);
// @RTGL1 - END
 varAU4(map1);
 map1[0]=AU1_AF1(toneScaleBias[0]);
 map1[1]=AU1_AF1(toneScaleBias[1]);
 map1[2]=AU1_AF1(lumaT[0]);
 map1[3]=AU1_AF1(lumaT[1]);
 // @RTGL1 - BEGIN
 LpmSetupOut(pContext, 1,map1);
 // @RTGL1 - END
 varAU4(map2);
 map2[0]=AU1_AF1(lumaT[2]);
 map2[1]=AU1_AF1(crosstalk[0]); 
 map2[2]=AU1_AF1(crosstalk[1]); 
 map2[3]=AU1_AF1(crosstalk[2]); 
 // @RTGL1 - BEGIN
 LpmSetupOut(pContext, 2,map2);
 // @RTGL1 - END
 varAU4(map3);
 map3[0]=AU1_AF1(rcpLumaT[0]);
 map3[1]=AU1_AF1(rcpLumaT[1]);
 map3[2]=AU1_AF1(rcpLumaT[2]);
 map3[3]=AU1_AF1(con2R[0]);
 // @RTGL1 - BEGIN
 LpmSetupOut(pContext, 3,map3);
 // @RTGL1 - END
 varAU4(map4);
 map4[0]=AU1_AF1(con2R[1]);
 map4[1]=AU1_AF1(con2R[2]);
 map4[2]=AU1_AF1(con2G[0]);
 map4[3]=AU1_AF1(con2G[1]);
 // @RTGL1 - BEGIN
 LpmSetupOut(pContext, 4,map4);
 // @RTGL1 - END
 varAU4(map5);
 map5[0]=AU1_AF1(con2G[2]);
 map5[1]=AU1_AF1(con2B[0]);
 map5[2]=AU1_AF1(con2B[1]);
 map5[3]=AU1_AF1(con2B[2]);
 // @RTGL1 - BEGIN
 LpmSetupOut(pContext, 5,map5);
 // @RTGL1 - END
 varAU4(map6);
 map6[0]=AU1_AF1(shoulderContrast);
 map6[1]=AU1_AF1(lumaW[0]);
 map6[2]=AU1_AF1(lumaW[1]);
 map6[3]=AU1_AF1(lumaW[2]);
 // @RTGL1 - BEGIN
 LpmSetupOut(pContext, 6,map6);
 // @RTGL1 - END
 varAU4(map7);
 map7[0]=AU1_AF1(softGap2[0]);
 map7[1]=AU1_AF1(softGap2[1]);
 map7[2]=AU1_AF1(conR[0]);
 map7[3]=AU1_AF1(conR[1]);
 // @RTGL1 - BEGIN
 LpmSetupOut(pContext, 7,map7);
 // @RTGL1 - END
 varAU4(map8);
 map8[0]=AU1_AF1(conR[2]);
 map8[1]=AU1_AF1(conG[0]);
 map8[2]=AU1_AF1(conG[1]);
 map8[3]=AU1_AF1(conG[2]);
 // @RTGL1 - BEGIN
 LpmSetupOut(pContext, 8,map8);
 // @RTGL1 - END
 varAU4(map9);
 map9[0]=AU1_AF1(conB[0]);
 map9[1]=AU1_AF1(conB[1]);
 map9[2]=AU1_AF1(conB[2]);
 map9[3]=AU1_(0);
 // @RTGL1 - BEGIN
 LpmSetupOut(pContext, 9,map9);
 // @RTGL1 - END
//-----------------------------------------------------------------------------------------------------------------------------
 // Packed 16-bit part of control block.
 varAU4(map16);varAF2(map16x);varAF2(map16y);varAF2(map16z);varAF2(map16w);
 map16x[0]=saturation[0];
 map16x[1]=saturation[1];
 map16y[0]=saturation[2];
 map16y[1]=contrast;
 map16z[0]=toneScaleBias[0];
 map16z[1]=toneScaleBias[1];
 map16w[0]=lumaT[0];
 map16w[1]=lumaT[1];
 map16[0]=AU1_AH2_AF2(map16x);
 map16[1]=AU1_AH2_AF2(map16y);
 map16[2]=AU1_AH2_AF2(map16z);
 map16[3]=AU1_AH2_AF2(map16w);
 // @RTGL1 - BEGIN
 LpmSetupOut(pContext, 16,map16);
 // @RTGL1 - END
 varAU4(map17);varAF2(map17x);varAF2(map17y);varAF2(map17z);varAF2(map17w);
 map17x[0]=lumaT[2];
 map17x[1]=crosstalk[0];
 map17y[0]=crosstalk[1];
 map17y[1]=crosstalk[2];
 map17z[0]=rcpLumaT[0];
 map17z[1]=rcpLumaT[1];
 map17w[0]=rcpLumaT[2];
 map17w[1]=con2R[0];
 map17[0]=AU1_AH2_AF2(map17x);
 map17[1]=AU1_AH2_AF2(map17y);
 map17[2]=AU1_AH2_AF2(map17z);
 map17[3]=AU1_AH2_AF2(map17w);
 // @RTGL1 - BEGIN
 LpmSetupOut(pContext, 17,map17);
 // @RTGL1 - END
 varAU4(map18);varAF2(map18x);varAF2(map18y);varAF2(map18z);varAF2(map18w);
 map18x[0]=con2R[1];
 map18x[1]=con2R[2];
 map18y[0]=con2G[0];
 map18y[1]=con2G[1];
 map18z[0]=con2G[2];
 map18z[1]=con2B[0];
 map18w[0]=con2B[1];
 map18w[1]=con2B[2];
 map18[0]=AU1_AH2_AF2(map18x);
 map18[1]=AU1_AH2_AF2(map18y);
 map18[2]=AU1_AH2_AF2(map18z);
 map18[3]=AU1_AH2_AF2(map18w);
 // @RTGL1 - BEGIN
 LpmSetupOut(pContext, 18,map18);
 // @RTGL1 - END
 varAU4(map19);varAF2(map19x);varAF2(map19y);varAF2(map19z);varAF2(map19w);
 map19x[0]=shoulderContrast;
 map19x[1]=lumaW[0];
 map19y[0]=lumaW[1];
 map19y[1]=lumaW[2];
 map19z[0]=softGap2[0];
 map19z[1]=softGap2[1];
 map19w[0]=conR[0];
 map19w[1]=conR[1];
 map19[0]=AU1_AH2_AF2(map19x);
 map19[1]=AU1_AH2_AF2(map19y);
 map19[2]=AU1_AH2_AF2(map19z);
 map19[3]=AU1_AH2_AF2(map19w);
 // @RTGL1 - BEGIN
 LpmSetupOut(pContext, 19,map19);
 // @RTGL1 - END
 varAU4(map20);varAF2(map20x);varAF2(map20y);varAF2(map20z);varAF2(map20w);
 map20x[0]=conR[2];
 map20x[1]=conG[0];
 map20y[0]=conG[1];
 map20y[1]=conG[2];
 map20z[0]=conB[0];
 map20z[1]=conB[1];
 map20w[0]=conB[2];
 map20w[1]=0.0;
 map20[0]=AU1_AH2_AF2(map20x);
 map20[1]=AU1_AH2_AF2(map20y);
 map20[2]=AU1_AH2_AF2(map20z);
 map20[3]=AU1_AH2_AF2(map20w);
 // @RTGL1 - BEGIN
 LpmSetupOut(pContext, 20,map20);}
 // @RTGL1 - END
