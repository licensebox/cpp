# Candidate CI reads only the checked-out public protocol contract. Production
# installs retain the declared stable release unless the CI path is explicit.
set(LICENSEBOX_C_CANDIDATE FALSE)
if(DEFINED ENV{LICENSEBOX_PROTOCOL_DIR} AND NOT "$ENV{LICENSEBOX_PROTOCOL_DIR}" STREQUAL "")
  file(READ "$ENV{LICENSEBOX_PROTOCOL_DIR}/contract.json" LICENSEBOX_CONTRACT)
  string(JSON LICENSEBOX_C_REF ERROR_VARIABLE LICENSEBOX_C_REF_ERROR GET "${LICENSEBOX_CONTRACT}" clients c candidate_ref)
  if(NOT LICENSEBOX_C_REF_ERROR)
    string(JSON LICENSEBOX_C_REF_TYPE TYPE "${LICENSEBOX_CONTRACT}" clients c candidate_ref)
    string(JSON LICENSEBOX_C_REPOSITORY GET "${LICENSEBOX_CONTRACT}" clients c repository)
    string(LENGTH "${LICENSEBOX_C_REF}" LICENSEBOX_C_REF_LENGTH)
    if(NOT LICENSEBOX_C_REF_TYPE STREQUAL "STRING" OR
       NOT LICENSEBOX_C_REPOSITORY STREQUAL "licensebox/c" OR
       NOT LICENSEBOX_C_REF_LENGTH EQUAL 40 OR NOT LICENSEBOX_C_REF MATCHES "^[0-9a-f]+$")
      message(FATAL_ERROR "Invalid fixed-repository C candidate pin")
    endif()
    set(LICENSEBOX_C_CANDIDATE TRUE)
    set(LICENSEBOX_C_VERSION "${LICENSEBOX_C_REF}")
  endif()
endif()
