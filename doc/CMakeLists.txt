file (GLOB dot_files *.dot)
set (dot_fig_pdfs)
foreach (_file ${dot_files})
  string (REPLACE ".dot" ".pdf" pdf_file ${_file})
  get_filename_component (pdf_file ${pdf_file} NAME)
  add_custom_command (
    OUTPUT ${pdf_file} 
    COMMAND dot -Tpdf ${_file}>${CMAKE_CURRENT_BINARY_DIR}/${pdf_file}
    DEPENDS ${_file}
  ) 
  list (APPEND dot_fig_pdfs ${pdf_file})
endforeach ()

add_custom_target (doc DEPENDS ${dot_fig_pdfs})
