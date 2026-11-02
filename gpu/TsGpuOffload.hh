//
// ********************************************************************
// *                                                                  *
// * Copyright 2026 The TOPAS Collaboration                           *
// *                                                                  *
// * Permission is hereby granted, free of charge, to any person      *
// * obtaining a copy of this software and associated documentation   *
// * files (the "Software"), to deal in the Software without          *
// * restriction, including without limitation the rights to use,     *
// * copy, modify, merge, publish, distribute, sublicense, and/or     *
// * sell copies of the Software, and to permit persons to whom the   *
// * Software is furnished to do so, subject to the following         *
// * conditions:                                                      *
// *                                                                  *
// * The above copyright notice and this permission notice shall be   *
// * included in all copies or substantial portions of the Software.  *
// *                                                                  *
// * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,  *
// * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES  *
// * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND         *
// * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT      *
// * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,     *
// * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING     *
// * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR    *
// * OTHER DEALINGS IN THE SOFTWARE.                                  *
// *                                                                  *
// ********************************************************************
//

#ifndef TsGpuOffload_hh
#define TsGpuOffload_hh

#include "globals.hh"

class TsParameterManager;
class G4VUserPhysicsList;

// Optional offload of electron, positron and gamma transport to Celeritas,
// which runs on a GPU (CUDA/HIP) or on the CPU (OpenMP) depending on how
// Celeritas was built. Controlled by the parameter b:Ts/UseGPU.
namespace TsGpuOffload
{
	// True when the user set Ts/UseGPU = "True"
	G4bool IsRequested(TsParameterManager* pM);

	// True when this TOPAS was compiled with Celeritas support
	G4bool IsAvailable();

	// Attach the Celeritas tracking manager to the physics list and pass it
	// the Ts/GPU/* options. Does nothing unless Ts/UseGPU is True.
	void RegisterWithPhysicsList(TsParameterManager* pM, G4VUserPhysicsList* physicsList);
}

#endif
