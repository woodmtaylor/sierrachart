#include "sierrachart.h"
SCDLLName("Momo Indy")

void HPF(SCStudyInterfaceRef sc, float src[], float output[], float lamb, int per)
{
    // Initialize variables
    float H1 = 0, H2 = 0, H3 = 0, H4 = 0, H5 = 0;
    float HH1 = 0, HH2 = 0, HH3 = 0, HH5 = 0;
    float HB = 0, HC = 0, Z = 0;

    float a[per], b[per], c[per];

    // Initialize arrays
    for (int i = 0; i < per; ++i)
    {
        a[i] = 0;
        b[i] = 0;
        c[i] = 0;
        output[i] = src[i];
    }

    // Set filter coefficients
    a[0] = 1.0 + lamb;
    b[0] = -2.0 * lamb;
    c[0] = lamb;

    for (int i = 1; i < per - 2; ++i)
    {
        a[i] = 6.0 * lamb + 1.0;
        b[i] = -4.0 * lamb;
        c[i] = lamb;
    }

    a[1] = 5.0 * lamb + 1;
    a[per - 1] = 1.0 + lamb;
    a[per - 2] = 5.0 * lamb + 1.0;
    b[per - 2] = -2.0 * lamb;

    // Filter loop
    for (int i = 0; i < per; ++i)
    {
        Z = a[i] - H4 * H1 - HH5 * HH2;
        if (Z == 0)
            break;

        HB = b[i];
        HH1 = H1;
        H1 = (HB - H4 * H2) / Z;
        b[i] = H1;

        HC = c[i];
        HH2 = H2;
        H2 = HC / Z;
        c[i] = H2;

        a[i] = (src[i] - HH3 * HH5 - H3 * H4) / Z;
        HH3 = H3;
        H3 = a[i];
        H4 = HB - H5 * HH1;
        HH5 = H5;
        H5 = HC;
    }

    H2 = 0;
    H1 = a[per - 1];
    output[per - 1] = H1;

    for (int i = per - 2; i >= 0; --i)
    {
        output[i] = a[i] - b[i] * H1 - c[i] * H2;
        H2 = H1;
        H1 = output[i];
    }
}

SCSFExport scsf_M(SCStudyInterfaceRef sc)
{
    //
    // Input Declaration
    //
    SCInputRef _userRenderBars = sc.Input[0];

    //
    // Subgraph Declaration
    //
    SCSubgraphRef MomentumLine = sc.Subgraph[0];

    if (sc.SetDefaults)
    {
        //
        // General
        //
        sc.GraphName = "M";
        sc.StudyDescription = "Momo Indy";

        //
        // Input
        //
        _userRenderBars.Name = "Bars to Render";
        _userRenderBars.SetInt(500);

        //
        // Subgraphs
        //
        MomentumLine.Name = "Momentum Line";
        
		MomentumLine.AutoColoring = AUTOCOLOR_SLOPE;
		MomentumLine.PrimaryColor = COLOR_WHITE;
		MomentumLine.SecondaryColor = COLOR_RED;
        MomentumLine.DrawStyle = DRAWSTYLE_LINE;
		MomentumLine.LineWidth = 2;
		MomentumLine.DrawZeros = 0;
		sc.GraphRegion = 0;
		
        return;
    }

    int barsToRender = _userRenderBars.GetInt();
    int HPsmoothPer = 15;
    float outputarr[barsToRender]; // Declare outputarr here

    // Get input data
    float close[barsToRender]; // Declare close here

    // Ensure sc.Close has enough elements
    int availableBars = sc.ArraySize;

    for (int i = 0; i < barsToRender; ++i) // Declare and initialize i here
    {
        close[i] = sc.Close[availableBars - barsToRender + i];
    }

    // Calculate momentum line using barsToRender
    HPF(sc, close, outputarr, 0.0625 / pow(sin(M_PI / HPsmoothPer), 4), barsToRender);

    // Initialize variables
    float out = 0;
    float sig = 0;
    int state = 0;
    bool goLong = false;
    bool goShort = false;
    COLORREF colorout = 0;

    float beta = 0, alpha = 0, m = 0, a = 0, b = 0, w = 0;
    float srcsample[barsToRender];

    for (int i = 0; i < barsToRender; ++i)
        srcsample[i] = 0;

    srcsample[0] = close[0]; // Assuming srcarr was meant to be close

    int nharm = 20;
    float frqtol = .01; 

    for (int harm = 1; harm <= nharm; ++harm)
    {
        alpha = 0;
        beta = 2;
        srcsample[0] = close[0] - outputarr[0]; // Assuming srcarr was meant to be close

        while (fabs(alpha - beta) > frqtol)
        {
            alpha = beta;
            srcsample[1] = close[1] - outputarr[1] + alpha * srcsample[0]; // Assuming srcarr was meant to be close
            float num = srcsample[0] * srcsample[1];
            float den = srcsample[0] * srcsample[0];

            for (int i = 2; i < barsToRender; ++i)
            {
                srcsample[i] = close[i] - outputarr[i] + alpha * srcsample[i - 1] - srcsample[i - 2]; // Assuming srcarr was meant to be close
                num += srcsample[i - 1] * (srcsample[i] + srcsample[i - 2]);
                den += srcsample[i - 1] * srcsample[i - 1];
            }

            beta = num / den;
        }

        w = acos(fmin(fmax(beta / 2.0, -1), 1));

        float Sc = 0, Ss = 0, Scc = 0, Sss = 0, Scs = 0, Sx = 0, Sxc = 0, Sxs = 0, den = 0;

        for (int i = 0; i < barsToRender; ++i)
        {
            float c = cos(w * i);
            float s = sin(w * i);
            float dx = close[i] - outputarr[i]; // Assuming srcarr was meant to be close
            Sc += c;
            Ss += s;
            Scc += c * c;
            Sss += s * s;
            Scs += c * s;
            Sx += dx;
            Sxc += dx * c;
            Sxs += dx * s;
        }

        Sc /= barsToRender;
        Ss /= barsToRender;
        Scc /= barsToRender;
        Sss /= barsToRender;
        Scs /= barsToRender;
        Sx /= barsToRender;
        Sxc /= barsToRender;
        Sxs /= barsToRender;

        if (w == 0)
        {
            m = Sx;
            a = 0;
            b = 0;
        }
        else
        {
            den = pow(Scs - Sc * Ss, 2) - (Scc - Sc * Sc) * (Sss - Ss * Ss);
            a = ((Sxs - Sx * Ss) * (Scs - Sc * Ss) - (Sxc - Sx * Sc) * (Sss - Ss * Ss)) / den;
            b = ((Sxc - Sx * Sc) * (Scs - Sc * Ss) - (Sxs - Sx * Ss) * (Scc - Sc * Sc)) / den;
            m = Sx - a * Sc - b * Ss;
        }

        for (int i = 0; i < barsToRender; ++i)
        {
            outputarr[i] += m + a * cos(w * i) + b * sin(w * i);
        }
		
    }

    // Store momentum line in subgraph
	char message[100];
	#include <cmath> // Include cmath for NaN definition

	// Inside the for loop
	for (int i = sc.UpdateStartIndex; i < sc.ArraySize; ++i)
	{
		if (i < sc.ArraySize - barsToRender)
		{
			// Set NaN values for bars before barsToRender
			MomentumLine[i] = 0;
		}
		else
		{
			// Plot calculated values for bars from barsToRender to the end
			MomentumLine[i] = outputarr[i - (sc.ArraySize - barsToRender)];
		}
		
	}


    //char message[100];
    // Debug: Log MomentumLine array
    //sc.AddMessageToLog("MomentumLine array (MomentumLine):", 1);
    //for (int i = 0; i < barsToRender; ++i)
    //{
     //   snprintf(message, 100, "MomentumLine[%d] = %f", i, MomentumLine[i]);
    //    sc.AddMessageToLog(message, 1);
	//	snprintf(message, 100, "OutputArr[%d] = %f", i, outputarr[i]);
    //    sc.AddMessageToLog(message, 1);
    //}

    return;
}