/******************************************************************************
 * @file investigation_dao.h
 * @brief Accès aux informations persistées de l'enquête.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_INVESTIGATION_DAO_H
#define LABFY_INVESTIGATION_INVESTIGATION_DAO_H

#include "database/database.h"
#include "models/investigation_record.h"

/**
 * @brief Charge l'unique enquête enregistrée dans la base.
 *
 * La fonction attend exactement une ligne dans la table investigation.
 *
 * @param database Connexion ouverte.
 *
 * @return Un nouveau modèle InvestigationRecord, ou NULL en cas d'échec.
 */
InvestigationRecord *investigation_dao_load(
    Database *database
);

#endif
