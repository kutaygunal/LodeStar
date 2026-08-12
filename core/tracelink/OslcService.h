#pragma once
// core/tracelink/OslcService.h
// OSLC (Open Services for Lifecycle Collaboration) integration for the
// TraceLink model (S2 Phase 12).
//
// Provides:
//   * an OSLC provider  -> exportRequirementAsOslc(): serializes a local
//     requirement as a standard OSLC RM requirement resource (RDF/XML) using
//     the dcterms and oslc_rm namespaces.
//   * an OSLC consumer  -> importRequirementFromOslc(): parses an OSLC
//     requirement resource and creates/updates a local requirement in the
//     TraceLink model.
//
// This enables interop with DOORS / Polarion / Codebeamer ecosystems that
// speak OSLC.

#include <string>

#include "core/common/Result.h"
#include "core/tracelink/TraceLinkService.h"

namespace lodestar::tracelink {

class OslcService {
public:
    explicit OslcService(TraceLinkService& svc);

    // --- OSLC provider -----------------------------------------------------
    // Serializes the requirement identified by `requirementId` (internal UUID)
    // as an OSLC RM requirement resource in RDF/XML. The resource carries the
    // requirement's dcterms:identifier and dcterms:title and is typed as an
    // oslc_rm:Requirement. Fails with NotFound if the requirement does not exist.
    common::Result<std::string> exportRequirementAsOslc(
        const std::string& requirementId);

    // --- OSLC consumer -----------------------------------------------------
    // Parses an OSLC requirement resource (RDF/XML) and creates a local
    // requirement in the TraceLink model whose external id and title match the
    // resource. Returns the created entity. Fails if the resource is malformed
    // or missing the required identifier/title.
    common::Result<Entity> importRequirementFromOslc(
        const std::string& oslcResource);

private:
    TraceLinkService& svc_;
};

}  // namespace lodestar::tracelink
