/******************************************************************************
 * @file entity_details_panel.h
 * @brief Volet latéral affichant les détails d'une entité.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_ENTITY_DETAILS_PANEL_H
#define LABFY_INVESTIGATION_ENTITY_DETAILS_PANEL_H

#include <gtk/gtk.h>
#include "models/entity_record.h"

G_BEGIN_DECLS

/**
 * @brief Modèle opaque représentant une entité.
 */
typedef struct EntityRecord EntityRecord;

/**
 * @brief Représentation opaque du volet de détails.
 */
typedef struct EntityDetailsPanel EntityDetailsPanel;

/**
 * @brief Callback appelé lorsque l'utilisateur ferme le volet.
 *
 * Le callback n'est pas déclenché lors d'une fermeture programmatique avec
 * entity_details_panel_clear().
 *
 * @param user_data Données empruntées fournies par l'appelant.
 */
typedef void (*EntityDetailsPanelCloseCallback)(
    gpointer user_data
);

/**
 * @brief Callback appelé lorsque l'utilisateur demande une relation.
 *
 * L'identifiant est emprunté et uniquement valide pendant l'appel.
 *
 * @param source_entity_identifier UUID de l'entité source.
 * @param user_data Données empruntées fournies par l'appelant.
 */
typedef void (*EntityDetailsPanelAddRelationCallback)(
    const char *source_entity_identifier,
    gpointer user_data
);

/** @brief Callback appelé lors du changement de catégorie d'une personne. */
typedef void (*EntityDetailsPanelPersonRoleCallback)(
    const char *entity_identifier,
    PersonRole role,
    gpointer user_data
);

/** @brief Callback appelé pour enregistrer la confiance d'une personne. */
typedef void (*EntityDetailsPanelPersonConfidenceCallback)(
    const char *entity_identifier,
    gint confidence,
    gpointer user_data
);

/** @brief Callback appelé pour enregistrer le nom affiché d'une personne. */
typedef void (*EntityDetailsPanelPersonNameCallback)(
    const char *entity_identifier,
    const char *display_name,
    gpointer user_data
);
/** @brief Callback appelé pour gérer les preuves d'une personne. */
typedef void (*EntityDetailsPanelPersonEvidenceCallback)(
    const char *entity_identifier, gpointer user_data);
/** @brief Callback appelé pour ouvrir les recherches OSINT compatibles. */
typedef void (*EntityDetailsPanelOsintCallback)(
    const char *entity_identifier, gpointer user_data);

/**
 * @brief Crée un volet de détails fermé.
 *
 * @return Nouveau volet, ou NULL en cas d'échec.
 */
EntityDetailsPanel *entity_details_panel_new(void);

/**
 * @brief Retourne le widget GTK racine du volet.
 *
 * Le widget appartient au module EntityDetailsPanel.
 *
 * @param details_panel Volet à consulter.
 *
 * @return Widget GTK racine, ou NULL.
 */
GtkWidget *entity_details_panel_get_widget(
    const EntityDetailsPanel *details_panel
);

/**
 * @brief Affiche une entité et ouvre le volet.
 *
 * Les textes sont immédiatement copiés dans les widgets GTK.
 * Le pointeur EntityRecord n'est jamais conservé.
 *
 * Passer NULL équivaut à entity_details_panel_clear().
 *
 * @param details_panel Volet à mettre à jour.
 * @param entity_record Entité empruntée, ou NULL.
 */
void entity_details_panel_set_entity(
    EntityDetailsPanel *details_panel,
    const EntityRecord *entity_record
);

/**
 * @brief Vide et referme le volet sans appeler le callback de fermeture.
 *
 * @param details_panel Volet à vider.
 */
void entity_details_panel_clear(
    EntityDetailsPanel *details_panel
);

/**
 * @brief Définit le callback du bouton de fermeture.
 *
 * Le callback et user_data sont empruntés.
 *
 * @param details_panel Volet à configurer.
 * @param callback Callback à appeler, ou NULL.
 * @param user_data Données empruntées du callback.
 */
void entity_details_panel_set_close_callback(
    EntityDetailsPanel *details_panel,
    EntityDetailsPanelCloseCallback callback,
    gpointer user_data
);

/**
 * @brief Définit le callback du bouton d'ajout d'une relation.
 *
 * Le volet transmet uniquement l'UUID de l'entité actuellement affichée.
 *
 * @param details_panel Volet à configurer.
 * @param callback Callback à appeler, ou NULL.
 * @param user_data Données empruntées du callback.
 */
void entity_details_panel_set_add_relation_callback(
    EntityDetailsPanel *details_panel,
    EntityDetailsPanelAddRelationCallback callback,
    gpointer user_data
);

/** @brief Définit le callback de changement de catégorie d'une personne. */
void entity_details_panel_set_person_role_callback(
    EntityDetailsPanel *details_panel,
    EntityDetailsPanelPersonRoleCallback callback,
    gpointer user_data
);

/**
 * @brief Définit le callback de modification de la confiance d'une personne.
 *
 * @param details_panel Volet à configurer.
 * @param callback Callback à appeler, ou NULL.
 * @param user_data Données empruntées du callback.
 */
void entity_details_panel_set_person_confidence_callback(
    EntityDetailsPanel *details_panel,
    EntityDetailsPanelPersonConfidenceCallback callback,
    gpointer user_data
);

/** @brief Définit le callback de modification du nom affiché. */
void entity_details_panel_set_person_name_callback(
    EntityDetailsPanel *details_panel,
    EntityDetailsPanelPersonNameCallback callback,
    gpointer user_data
);
/** @brief Définit le callback de gestion des preuves d'une personne. */
void entity_details_panel_set_person_evidence_callback(
    EntityDetailsPanel *details_panel,
    EntityDetailsPanelPersonEvidenceCallback callback, gpointer user_data);
/** @brief Définit le callback du bouton de recherche OSINT. */
void entity_details_panel_set_osint_callback(EntityDetailsPanel *details_panel,
    EntityDetailsPanelOsintCallback callback, gpointer user_data);
/**
 * @brief Affiche les preuves associées à la personne courante.
 * @param details_panel Volet à mettre à jour.
 * @param evidence_records Tableau emprunté de EvidenceRecord, ou NULL.
 */
void entity_details_panel_set_person_evidences(
    EntityDetailsPanel *details_panel, const GPtrArray *evidence_records);

/**
 * @brief Indique si le volet est actuellement ouvert.
 *
 * @param details_panel Volet à consulter.
 *
 * @return TRUE si le volet est ouvert, sinon FALSE.
 */
gboolean entity_details_panel_is_open(
    const EntityDetailsPanel *details_panel
);

/**
 * @brief Libère la structure d'encapsulation du volet.
 *
 * Cette fonction accepte NULL.
 *
 * @param details_panel Volet à libérer.
 */
void entity_details_panel_free(
    EntityDetailsPanel *details_panel
);

G_END_DECLS

#endif
