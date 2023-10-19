mergeInto(LibraryManager.library, {
  publishInputNumber: function(input_number) {
    if (typeof Module.publishInputNumber === "function")
      Module.publishInputNumber(UTF8ToString(input_number));
  },
  publishFactor: function(factor_type, factor_number) {
    if (typeof Module.publishFactor === "function")
      Module.publishFactor(factor_type, UTF8ToString(factor_number));
  },
  publishState: function(num_relations, full_relations, combined_relations, partial_relations, max_relations) {
    if (typeof Module.publishState === "function")
      Module.publishState(num_relations, full_relations, combined_relations, partial_relations, max_relations);
  },
});
