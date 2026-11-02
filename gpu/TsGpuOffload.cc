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

#include "TsGpuOffload.hh"

#include "TsParameterManager.hh"
#include "TsTopasConfig.hh"

#include "G4VModularPhysicsList.hh"
#include "G4PhysicalVolumeStore.hh"
#include "G4VPhysicalVolume.hh"

#include <set>

#ifdef TOPAS_WITH_CELERITAS
#include <CeleritasG4.hh>
#endif

G4bool TsGpuOffload::IsRequested(TsParameterManager* pM)
{
	return pM->ParameterExists("Ts/UseGPU") && pM->GetBooleanParameter("Ts/UseGPU");
}

G4bool TsGpuOffload::IsAvailable()
{
#ifdef TOPAS_WITH_CELERITAS
	return true;
#else
	return false;
#endif
}

void TsGpuOffload::RegisterWithPhysicsList(TsParameterManager* pM, G4VUserPhysicsList* physicsList)
{
	if (!IsRequested(pM))
		return;

	if (!IsAvailable()) {
		G4cerr << "Topas is exiting due to a serious error in physics setup." << G4endl;
		G4cerr << "Ts/UseGPU is True, but this TOPAS was built without GPU support." << G4endl;
		G4cerr << "Rebuild TOPAS with -DTOPAS_WITH_CELERITAS=ON, or set Ts/UseGPU to False." << G4endl;
		pM->AbortSession(1);
		return;
	}

	// Fields are not passed to Celeritas: offloaded tracks would ignore them
	std::vector<G4String> fieldParameters;
	pM->GetParameterNamesBracketedBy("Ge/", "/Field", &fieldParameters);
	if (!fieldParameters.empty()) {
		G4cerr << "Topas is exiting due to a serious error in physics setup." << G4endl;
		G4cerr << "Ts/UseGPU is True, but your geometry has an electric or magnetic field:" << G4endl;
		for (const G4String& name : fieldParameters)
			G4cerr << "    " << name << G4endl;
		G4cerr << "Fields are not yet supported with Ts/UseGPU. Remove the field or set Ts/UseGPU to False." << G4endl;
		pM->AbortSession(1);
		return;
	}

	// Celeritas converts parameterised volumes using only each copy's position,
	// not its size or material (TOPAS uses them for radial/angular bins, voxel
	// materials and patient images), and the same applies to replicas along
	// rho or phi. Such geometries would give wrong results.
	std::set<G4String> unsupportedVolumes;
	for (G4VPhysicalVolume* pv : *G4PhysicalVolumeStore::GetInstance()) {
		if (pv->IsParameterised()) {
			unsupportedVolumes.insert(pv->GetName() + " (parameterised)");
		} else if (pv->IsReplicated()) {
			EAxis axis;
			G4int nReplicas;
			G4double width, offset;
			G4bool consuming;
			pv->GetReplicationData(axis, nReplicas, width, offset, consuming);
			if (axis != kXAxis && axis != kYAxis && axis != kZAxis)
				unsupportedVolumes.insert(pv->GetName() + " (replicated along rho or phi)");
		}
	}
	if (!unsupportedVolumes.empty()) {
		G4cerr << "Topas is exiting due to a serious error in physics setup." << G4endl;
		G4cerr << "Ts/UseGPU is True, but your geometry contains volumes that Celeritas cannot yet handle:" << G4endl;
		for (const G4String& name : unsupportedVolumes)
			G4cerr << "    " << name << G4endl;
		G4cerr << "These come from R or Phi bins (TsCylinder, TsSphere), VoxelMaterials or patient images." << G4endl;
		G4cerr << "Use only X, Y or Z bins, or set Ts/UseGPU to False." << G4endl;
		pM->AbortSession(1);
		return;
	}

	G4VModularPhysicsList* modularList = dynamic_cast<G4VModularPhysicsList*>(physicsList);
	if (!modularList) {
		G4cerr << "Topas is exiting due to a serious error in physics setup." << G4endl;
		G4cerr << "Ts/UseGPU requires a modular physics list (Geant4_Modular or a Geant4 reference list)." << G4endl;
		pM->AbortSession(1);
		return;
	}

#ifdef TOPAS_WITH_CELERITAS
	celeritas::TrackingManagerIntegration& tmi = celeritas::TrackingManagerIntegration::Instance();

	// Send e-, e+ and gamma to Celeritas instead of the Geant4 tracking loop
	modularList->RegisterPhysics(new celeritas::TrackingManagerConstructor(&tmi));

	celeritas::SetupOptions options;

	// Number of tracks transported in parallel per thread. The defaults suit CPU
	// execution; on a GPU, values of several hundred thousand are typical.
	options.max_num_tracks = 2048;
	if (pM->ParameterExists("Ts/GPU/MaxNumTracks"))
		options.max_num_tracks = pM->GetIntegerParameter("Ts/GPU/MaxNumTracks");

	options.initializer_capacity = 128 * options.max_num_tracks;
	if (pM->ParameterExists("Ts/GPU/InitializerCapacity"))
		options.initializer_capacity = pM->GetIntegerParameter("Ts/GPU/InitializerCapacity");

	// Celeritas does not support single Coulomb scattering, which Geant4 uses
	// alongside multiple scattering at high energy
	options.ignore_processes = {"CoulombScat"};
	if (pM->ParameterExists("Ts/GPU/IgnoreProcesses")) {
		options.ignore_processes.clear();
		G4String* processes = pM->GetStringVector("Ts/GPU/IgnoreProcesses");
		G4int nProcesses = pM->GetVectorLength("Ts/GPU/IgnoreProcesses");
		for (G4int i = 0; i < nProcesses; ++i)
			options.ignore_processes.push_back(processes[i]);
	}

	// No field: setups with fields are rejected above
	options.make_along_step = celeritas::UniformAlongStepFactory();

	// Send every step in a scored volume back to TOPAS, including steps that
	// deposit no energy: fluence and phase-space scorers need them
	options.sd.ignore_zero_deposition = false;

	options.output_file = "celeritas.out.json";
	if (pM->ParameterExists("Ts/GPU/OutputFile"))
		options.output_file = pM->GetStringParameter("Ts/GPU/OutputFile");

	// Optional dump of the geometry as converted by Celeritas, for debugging
	if (pM->ParameterExists("Ts/GPU/GeometryOutputFile"))
		options.geometry_output_file = pM->GetStringParameter("Ts/GPU/GeometryOutputFile");

	tmi.SetOptions(std::move(options));

	G4cout << "TOPAS: electron, positron and gamma transport offloaded to Celeritas" << G4endl;
#endif
}
