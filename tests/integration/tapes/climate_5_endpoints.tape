# Servirtium tape for contrib.climate_http_tests step-2 playback.
#
# Five interactions cover every endpoint the climate API tests hit:
#   0: gbr 1980-1999 (200, full XML)
#   1: fra 1980-1999 (200, full XML)
#   2: egy 1980-1999 (200, full XML)
#   3: gbr 1985-1995 (200, "<list/>" → maps to "date range not supported")
#   4: mde 1980-1999 (200, "Invalid country code" → maps to "bad country code")
#
# Bodies were captured verbatim from the GitHub-Pages mirror at
# https://servirtium.github.io/worldbank-climate-recordings on 2026-04-25.
# This means they're frozen — if the mirror ever changes, the tape
# stays put. That's the entire point of a tape (deterministic, offline,
# fast, doesn't depend on the upstream staying available).

## Interaction 0: GET /climateweb/rest/v1/country/annualavg/pr/1980/1999/gbr.xml

### Request headers recorded for playback:

```
Host: 127.0.0.1:18099
```

### Request body recorded for playback ():

```
```

### Response headers recorded for playback:

```
Content-Type: application/xml
```

### Response body recorded for playback (200: application/xml):

```
<list> <domain.web.AnnualGcmDatum> <gcm>bccr_bcm2_0</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>987.9504418944</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>cccma_cgcm3_1</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>815.2627636718801</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>cnrm_cm3</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>1099.3898999037601</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>csiro_mk3_5</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>1021.6996069333198</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>gfdl_cm2_0</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>1019.8750146478401</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>gfdl_cm2_1</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>1084.5603759764</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>ingv_echam4</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>1008.2985131833999</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>inmcm3_0</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>1194.9564575200002</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>ipsl_cm4</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>893.9680444336799</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>miroc3_2_medres</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>1032.85460449136</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>miub_echo_g</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>905.9324633786798</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>mpi_echam5</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>1024.2805590819598</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>mri_cgcm2_3_2a</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>784.5488305664002</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>ukmo_hadcm3</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>957.3522631840398</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>ukmo_hadgem1</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>1001.7526196294</double> </annualData> </domain.web.AnnualGcmDatum> </list> 
```

## Interaction 1: GET /climateweb/rest/v1/country/annualavg/pr/1980/1999/fra.xml

### Request headers recorded for playback:

```
Host: 127.0.0.1:18099
```

### Request body recorded for playback ():

```
```

### Response headers recorded for playback:

```
Content-Type: application/xml
```

### Response body recorded for playback (200: application/xml):

```
<list> <domain.web.AnnualGcmDatum> <gcm>bccr_bcm2_0</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>1077.8620827419693</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>cccma_cgcm3_1</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>745.3185434053028</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>cnrm_cm3</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>1063.867595326212</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>csiro_mk3_5</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>1007.0295299183938</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>gfdl_cm2_0</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>1061.3426458178785</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>gfdl_cm2_1</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>1004.4105613005454</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>ingv_echam4</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>750.6408894859092</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>inmcm3_0</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>967.5554180724546</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>ipsl_cm4</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>873.0839862248788</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>miroc3_2_medres</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>903.9068381571817</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>miub_echo_g</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>911.8733705463638</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>mpi_echam5</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>807.6978963218183</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>mri_cgcm2_3_2a</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>643.4242285526061</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>ukmo_hadcm3</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>911.9471435543941</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>ukmo_hadgem1</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>977.019703258182</double> </annualData> </domain.web.AnnualGcmDatum> </list> 
```

## Interaction 2: GET /climateweb/rest/v1/country/annualavg/pr/1980/1999/egy.xml

### Request headers recorded for playback:

```
Host: 127.0.0.1:18099
```

### Request body recorded for playback ():

```
```

### Response headers recorded for playback:

```
Content-Type: application/xml
```

### Response body recorded for playback (200: application/xml):

```
<list> <domain.web.AnnualGcmDatum> <gcm>bccr_bcm2_0</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>86.9453507501108</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>cccma_cgcm3_1</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>88.30762904406757</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>cnrm_cm3</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>106.36732833443784</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>csiro_mk3_5</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>28.62417012292162</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>gfdl_cm2_0</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>45.26438762045865</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>gfdl_cm2_1</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>26.459523922684326</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>ingv_echam4</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>26.899282784094865</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>inmcm3_0</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>46.921809016059456</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>ipsl_cm4</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>21.791464905471624</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>miroc3_2_medres</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>78.83864541954053</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>miub_echo_g</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>75.66823740260838</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>mpi_echam5</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>31.494514729511625</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>mri_cgcm2_3_2a</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>35.64848946636978</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>ukmo_hadcm3</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>56.096262184318924</double> </annualData> </domain.web.AnnualGcmDatum> <domain.web.AnnualGcmDatum> <gcm>ukmo_hadgem1</gcm> <variable>pr</variable> <fromYear>1980</fromYear> <toYear>1999</toYear> <annualData> <double>63.461061116817845</double> </annualData> </domain.web.AnnualGcmDatum></list>
```

## Interaction 3: GET /climateweb/rest/v1/country/annualavg/pr/1985/1995/gbr.xml

### Request headers recorded for playback:

```
Host: 127.0.0.1:18099
```

### Request body recorded for playback ():

```
```

### Response headers recorded for playback:

```
Content-Type: application/xml
```

### Response body recorded for playback (200: application/xml):

```
<list/>

```

## Interaction 4: GET /climateweb/rest/v1/country/annualavg/pr/1980/1999/mde.xml

### Request headers recorded for playback:

```
Host: 127.0.0.1:18099
```

### Request body recorded for playback ():

```
```

### Response headers recorded for playback:

```
Content-Type: application/xml
```

### Response body recorded for playback (200: application/xml):

```
Invalid country code. Three letters are required
```

